from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import collections
import copy
import json
import os
import random
import time
from typing import List, Tuple

import numpy as np

import tensorflow as tf
import sonnet as snt
import graph_nets as gn

import actclass as ac


os.environ['TF_CPP_MIN_LOG_LEVEL'] = '2'


class TrainState:
    def __init__(self, basepath: str):
        self.filename = os.path.join(basepath, 'train_state.json')
        self.timestamps = []
        self.logged_iterations = []
        self.losses_tr = []
        self.losses_ge = []
        self.cors_tr = []
        self.cors_ge = []
        self.cors3_tr = []
        self.cors3_ge = []
        self.cm_ground_truth = []
        self.cm_top1_pred = []
        self.cm_top3_pred = []

    def log_validation(self, timestamp: float, iteration_number: int, loss_tr: float, loss_ge: float,
                       cor_tr: float, cor_ge: float, cor3_tr: float, cor3_ge: float,
                       cm_ground_truth: List[int], cm_top_1: List[int], cm_top_3: List[int]) -> None:
        assert len(cm_ground_truth) == len(cm_top_1) == len(cm_top_3)
        self.timestamps.append(timestamp)
        self.logged_iterations.append(iteration_number)
        self.losses_tr.append(loss_tr)
        self.losses_ge.append(loss_ge)
        self.cors_tr.append(cor_tr)
        self.cors_ge.append(cor_ge)
        self.cors3_tr.append(cor3_tr)
        self.cors3_ge.append(cor3_ge)
        self.cm_ground_truth.append(cm_ground_truth)
        self.cm_top1_pred.append(cm_top_1)
        self.cm_top3_pred.append(cm_top_3)

    def numpy_confusions(self):
        true = np.array(self.cm_ground_truth, dtype=np.int64).reshape(-1)
        pred1 = np.array(self.cm_top1_pred, dtype=np.int64).reshape(-1)
        pred3 = np.array(self.cm_top3_pred, dtype=np.int64).reshape(-1)
        return true, pred1, pred3

    def persist_state(self) -> None:
        written = False
        while not written:
            try:
                with open(self.filename, 'w') as outfile:
                    json.dump(self.__dict__, outfile)
                ac.print3('Written train state to `{}`.'.format(self.filename))
            except Exception as e:
                print(e)
            else:
                written = True

    def restore_state(self, global_step: int) -> None:
        read = False
        while not read:
            data = None
            try:
                with open(self.filename) as json_file:
                    data = json.load(json_file)
            except Exception as e:
                print(e)
            else:
                for i, global_step_i in enumerate(data['logged_iterations']):
                    if global_step_i > global_step:
                        ac.print3('Skipped iteration {} while restoring for global_step {}'.format(
                            global_step_i, global_step))
                        continue
                    self.timestamps.append(data['timestamps'][i])
                    self.logged_iterations.append(data['logged_iterations'][i])
                    self.losses_tr.append(data['losses_tr'][i])
                    self.losses_ge.append(data['losses_ge'][i])
                    self.cors_tr.append(data['cors_tr'][i])
                    self.cors_ge.append(data['cors_ge'][i])
                    self.cors3_tr.append(data['cors3_tr'][i])
                    self.cors3_ge.append(data['cors3_ge'][i])
                    self.cm_ground_truth.append(data['cm_ground_truth'][i])
                    self.cm_top1_pred.append(data['cm_top1_pred'][i])
                    self.cm_top3_pred.append(data['cm_top3_pred'][i])
                read = True
        ac.print3('Done restoring train state.')


class ActionClassifierModel:

    def __init__(self, out_path: str, processing_steps_count: int = 10, layer_count: int = 2,
                 neuron_count: int = 32):
        """
        Initialises a model instance to train or make predictions.

        :param out_path: General output folder.
        """
        tf.compat.v1.reset_default_graph()

        # Model parameters.
        self.processing_steps_count: int = processing_steps_count

        # Instance members.
        self.out_path: str = out_path
        self.model = EncodeProcessDecode(layer_count=layer_count, neuron_count=neuron_count,
                                         edge_output_size=len(ac.relations),
                                         node_output_size=len(ac.objects),
                                         global_output_size=len(ac.actions))
        self.session: tf.Session | None = None
        self.saver: tf.train.Saver | None = None

    def train(self, train_set: List[ac.dataset.SceneGraphProxy], valid_set: List[ac.dataset.SceneGraphProxy],
              restore=None, batch_size_train: int = 512, batch_size_valid: int = 1024, learning_rate: float = 0.001,
              max_iteration: int = 3000, log_interval: int = 120, save_interval: int = 250) -> None:
        """
        Trains the model given an train_set and validates it on valid_set.

        :param train_set: Training set.
        :param valid_set: Validation set.
        :param restore: Iteration number of the state to restore.
        :param batch_size_train: Train batch size.
        :param batch_size_valid: Validation batch size.
        :param learning_rate: Learning rate.
        :param max_iteration: Max iteration (aborting afterwards).
        :param log_interval: Interval when a log should be printed to stdout.
        :param save_interval: Interval when the model should be saved to disk.
        """
        print('')
        ac.print2('Starting training.')
        ac.print2('configuration:')
        ac.print2(' - restore: {}'.format(restore))
        ac.print2(' - batch_size_train: {}'.format(batch_size_train))
        ac.print2(' - batch_size_valid: {}'.format(batch_size_valid))
        ac.print2(' - learning_rate: {}'.format(learning_rate))
        ac.print2(' - max_iteration: {}'.format(max_iteration))
        ac.print2(' - log_interval: {}'.format(log_interval))
        ac.print2(' - save_interval: {}'.format(save_interval))

        # Create output directories.
        os.makedirs(os.path.join(self.out_path, 'confusion_matrices'), exist_ok=True)
        os.makedirs(os.path.join(self.out_path, 'losses'), exist_ok=True)
        os.makedirs(os.path.join(self.out_path, 'models'), exist_ok=True)

        # Data.
        # Input and target placeholders.
        placeholder = [train_set[0].load()]
        input_ph = gn.utils_tf.placeholders_from_data_dicts(placeholder)   # input graph: for edge, node
        target_ph = gn.utils_tf.placeholders_from_data_dicts(placeholder)  # target graph: for global attribute

        # A list of outputs, one per processing step.
        output_ops_tr = self.model(input_ph, self.processing_steps_count)  # send input graph into model and has output graphs
        # tr: train
        output_ops_ge = self.model(input_ph, self.processing_steps_count)
        # ge: generalization

        # Training loss.
        loss_ops_tr = self._create_loss_ops(target_ph, output_ops_tr)      # losses between target & output global attribute of each processing step
        # Loss across processing steps.
        loss_op_tr = sum(loss_ops_tr) / self.processing_steps_count        # average loss as training loss
        # Generalization loss.
        loss_ops_ge = self._create_loss_ops(target_ph, output_ops_ge)
        loss_op_ge = loss_ops_ge[-1]                                       # loss of last training step as generalization loss

        # Global step variable.
        global_step = tf.Variable(0, name='global_step', trainable=False)

        # Optimizer.
        optimizer = tf.train.AdamOptimizer(learning_rate)
        step_op = optimizer.minimize(loss_op_tr, global_step=global_step)

        # Lets an iterable of TF graphs be output from a session as NP graphs.
        input_ph, target_ph = self._make_all_runnable_in_session(input_ph, target_ph)

        # Initialise and restore session if desired.
        self._init_session()
        train_state = TrainState(self.saver_basepath)
        iteration_no: int = 0
        if restore:
            self._restore_session(restore)
            train_state.restore_state(restore)
            iteration_no = self.session.run(global_step)
            ac.print2('Model restored from `{}` at iteration #{}.'.format(self.saver_path, iteration_no))

        print('')
        ac.print1('Legend:')
        ac.print1('# (iteration number), T (elapsed seconds), '
                  'Ltr (training loss), Lge (generalisation loss), '
                  'Ctr (correct mean, train), '
                  'Cge (correct mean, generalisation), '
                  'C3tr (top3 guess mean, train), '
                  'C3ge (top3 guess mean, generalisation)')

        start_time = time.time()
        last_log_time = start_time
        # Iterate as long as max_iteration is not reached.  If max_iteration is set to a negative value, keep iterating
        # forever.
        while iteration_no <= max_iteration or max_iteration < 0:
            # Run session.
            iteration_no = self.session.run(global_step)
            feed_dict_tr = self._create_train_feed_dict(iteration_no, train_set, batch_size_train, input_ph, target_ph)
            # training deed dict: contains filtered scenegraphs without groundtruth und scenegraphs with groundtruth for current batch
            run_params_tr = {
                'step': step_op,  # step_op is objective of the training step
                'targets': target_ph,
                'loss': loss_op_tr,
                'outputs': output_ops_tr,
            }
            train_values = self.session.run(run_params_tr, feed_dict=feed_dict_tr)  # returned value is a dictionary

            # Save model (only if not the first iteration after starting).
            if iteration_no % save_interval == 0 and iteration_no not in [0, restore]:
                self._persist_session(iteration_no)
                train_state.persist_state()
                ac.print2('Model saved to `{}` for iteration #{}.'.format(self.saver_path, iteration_no))

            # Assess timings.
            the_time = time.time()
            elapsed_since_last_log = the_time - last_log_time

            # Validate.
            if elapsed_since_last_log > log_interval:
                last_log_time = the_time
                feed_dict_ge = self._create_train_feed_dict(-1, valid_set, batch_size_valid, input_ph, target_ph)
                run_params_ge = {
                    'targets': target_ph,
                    'loss': loss_op_ge,
                    'outputs': output_ops_ge,
                }
                test_values = self.session.run(run_params_ge, feed_dict=feed_dict_ge)  # contains frames of whole validation batch
                cor_tr, cor3_tr = self._compute_accuracy(train_values['targets'], train_values['outputs'][-1])  # training accuracy between groundtruth and last step processing result
                cor_ge, cor3_ge = self._compute_accuracy(test_values['targets'], test_values['outputs'][-1])
                elapsed = time.time() - start_time
                loss_tr = train_values['loss']
                loss_ge = test_values['loss']

                log_line = '# {:05d}, T {:.1f}, Ltr {:.4f}, Lge {:.4f}, ' \
                           'Ctr {:.4f}, Cge {:.4f}, C3tr {:.4f}, C3ge {:.4f}'
                ac.print1(log_line.format(iteration_no, elapsed, loss_tr, loss_ge, cor_tr, cor_ge, cor3_tr, cor3_ge))

                # Confusion matrix.
                cm_gt, cm_top_1, cm_top_3 = self._compute_confusions(test_values['targets'], test_values['outputs'][-1])    # list of groundtruth, top1 prediction, top3 prediction of the validation batch
                train_state.log_validation(elapsed, int(iteration_no), float(loss_tr), float(loss_ge),
                                           float(cor_tr), float(cor_ge), float(cor3_tr), float(cor3_ge),
                                           list(cm_gt.tolist()), list(cm_top_1.tolist()), list(cm_top_3.tolist()))

                true, pred1, pred3 = train_state.numpy_confusions()  # reshape(-1)
                classes = np.array(ac.actions)
                filename_conf1 = os.path.join(self.out_path, 'confusion_matrices',
                                              'conf_top1_{}.png'.format(iteration_no))
                filename_conf3 = os.path.join(self.out_path, 'confusion_matrices',
                                              'conf_top3_{}.png'.format(iteration_no))
                ac.plot.confusion_matrix(true, pred1, classes, filename_conf1, normalize=True)
                ac.plot.confusion_matrix(true, pred3, classes, filename_conf3, normalize=True)

                # Loss graph.
                filename = os.path.join(self.out_path, 'losses', 'loss_{}.png'.format(iteration_no))
                ac.plot.loss_graph(train_state.logged_iterations, train_state.losses_tr, train_state.losses_ge,
                                   filename)

    def predict(self, test_set: List[ac.dataset.SceneGraphProxy], restore) -> None:
        # Create output directories.
        os.makedirs(os.path.join(self.out_path, 'predictions'), exist_ok=False)

        # Model.
        placeholder = [test_set[0].load()]
        input_ph = gn.utils_tf.placeholders_from_data_dicts(placeholder)
        output_ops = self.model(input_ph, self.processing_steps_count)

        # Init and restore session.
        self._init_session()
        self._restore_session(restore)  # restore the model from given iteration number
        ac.print3('Model restored from `{}`.'.format(self.saver_path))

        times: List[float] = []  # To assess timings.
        current_take = {'right': [], 'left': []}
        global_frame_id: int = 0
        current_frame: ac.dataset.SceneGraphProxy = test_set[global_frame_id]  # load scengraph from idex 0 in test set

        # Helper function to check if the current frame is the last frame in this take.
        def is_last_frame_in_take():
            try:
                return test_set[global_frame_id + 1].take != test_set[global_frame_id].take
            except IndexError:
                return True

        ac.print2('Looping over test set now.')
        has_next_frame: bool = True
        while has_next_frame:                                                   # repeat it until there is no new frame
            start_time = time.time()
            # Get predictions and append them to list.
            graph = current_frame.load()

            # Empty graphs will cause the framework to crash.
            if len(graph['edges']) > 0:
                feed_dict = self._create_predict_feed_dict(graph, input_ph)
                run_params_test = {
                    'outputs': output_ops
                }
                values = self.session.run(run_params_test, feed_dict=feed_dict)
                output = gn.utils_np.graphs_tuple_to_data_dicts(values['outputs'][-1])
                assert len(output) == 1
                output = output[0]['globals']
                current_take[current_frame.side].append(output.tolist())        # output -> probability of each action index
                times.append(time.time() - start_time)
            else:
                current_take[current_frame.side].append([0] * len(ac.actions))  # if the edge size is 0, no action recognized
                times.append(time.time() - start_time)

            # Persist predictions if this was the last frame.
            if is_last_frame_in_take():
                assert len(current_take['right']) == len(current_take['left'])
                identifier = 's{}_ts{}_tk{}'.format(current_frame.subject, current_frame.task, current_frame.take)
                ac.print2('Writing predictions for `{}` ({} frames).'.format(identifier, len(current_take['right'])))
                filename = 'predictions_{}.json'.format(identifier)
                with open(os.path.join(self.out_path, 'predictions', filename), 'w') as fp:
                    json.dump(current_take, fp)     # write action of left and right side of each take into file
                current_take['right'] = []          # reset the action list
                current_take['left'] = []

            # Try to load next frame or exit if it fails.
            global_frame_id += 1
            try:
                current_frame = test_set[global_frame_id]
            except IndexError:
                ac.print1('Reached the end of the test set.  Exiting main loop now.')
                has_next_frame = False
        ac.print1('Predicting took {} ms on average'.format(np.average(times) * 1000))

    def _init_session(self) -> None:
        if self.session:
            self.session.close()

        self.session = tf.Session()
        self.session.run(tf.global_variables_initializer())

        self.saver = tf.train.Saver(max_to_keep=None)
        self.saver_basepath = os.path.join(self.out_path, 'models',)
        self.saver_path = os.path.join(self.saver_basepath, 'model.ckpt')

    def _persist_session(self, iteration_no: int) -> None:
        self.saver.save(self.session, self.saver_path, global_step=iteration_no)

    def _restore_session(self, iteration_no: int) -> None:
        self.saver.restore(self.session, '{p}-{i}'.format(p=self.saver_path, i=iteration_no))

    @staticmethod
    def _create_loss_ops(target_op, output_ops):
        if not isinstance(output_ops, collections.Sequence):
            raise Exception('Is not a collection!')
        loss_ops = [
            tf.losses.softmax_cross_entropy(target_op.globals, output_op.globals) for output_op in output_ops
        ]
        return loss_ops

    @staticmethod
    def _make_all_runnable_in_session(*args):
        """Lets an iterable of TF graphs be output from a session as NP graphs."""
        return [gn.utils_tf.make_runnable_in_session(a) for a in args]

    @staticmethod
    def _create_train_feed_dict(iteration, dataset, batch_size, input_ph, target_ph):
        """Creates placeholders for the model training and evaluation.

        :param iteration: Current iteration index.
        :param dataset: The dataset the batch should be extracted from.
        :param batch_size: Total number of graphs per batch.
        :param input_ph: The input graph's placeholders, as a graph namedtuple.
        :param target_ph: The target graph's placeholders, as a graph namedtuple.

        :return feed_dict: The feed `dict` of input and target placeholders and data.
        """

        # print('Loading dataset...')
        if iteration < 0:
            random.shuffle(dataset)
            inputs = [f.load() for f in dataset[:batch_size]]
        else:
            lower = (iteration * batch_size) % len(dataset)
            upper = lower + batch_size
            if upper > len(dataset):
                lower = 0
                upper = batch_size
                random.shuffle(dataset)
            inputs = [f.load() for f in dataset[lower:upper]]
            assert len(inputs) == batch_size
        # print('Done loading')

        def filter_labels(ds):
            # pre-process dataset.
            new_ds = []
            idle_counter: int = 0
            hold_counter: int = 0
            for t in ds:
                # Drop unlabelled.
                if np.sum(t['globals']) == 0:
                    continue
                assert np.sum(t['globals']) == 1

                action = ac.actions[np.argmax(t['globals'])]  # because the saved information is in one-hot encoding
                # Drop 2 of 3 idles.
                if action == 'idle':
                    idle_counter += 1
                    if idle_counter % 3 != 0:
                        continue
                # Drop 2 of 3 holds
                elif action == 'hold':
                    hold_counter += 1
                    if hold_counter % 3 != 0:
                        continue
                # because idle and hold posses too many frames, we drop those to decrease the effect of this two action

                new_ds.append(t)
            return new_ds

        # Filter out every 2 of 3 hold and idle actions for train dataset.
        if iteration >= 0:
            inputs = filter_labels(inputs)

        # Graphs with no edges may make the network crash.  Filter them before proceeding.
        for i in inputs:
            assert len(i['edges']) == len(i['senders']) == len(i['receivers'])
        inputs = [i for i in inputs if len(i['edges']) > 0]     # every inputs should have edges, otherwise it doesn't work
        if len(inputs) == 0:
            assert False, 'should not happen'
            # inputs = copy.deepcopy(dataset[0:1])

        # The validation set is prone to size fluctuations.  Ensure the batch size.
        if iteration < 0:
            while len(inputs) != batch_size:
                random.shuffle(dataset)
                candidate = dataset[0].load()
                if len(candidate['edges']) > 0:
                    inputs.append(candidate)
            assert len(inputs) == batch_size

        # Create ground truth copy.
        targets = copy.deepcopy(inputs)

        # Censor ground truth for input graphs.
        # because for input graph we don't want it to have groundtruth, we want to train it.
        for input in inputs:
            input['globals'] = [0.] * len(ac.actions)

        input_graphs = gn.utils_np.data_dicts_to_graphs_tuple(inputs)    # input doens't contain groundtruth because it uses as features
        target_graphs = gn.utils_np.data_dicts_to_graphs_tuple(targets)  # target contrains groundtruth because it uses as label

        feed_dict = {input_ph: input_graphs, target_ph: target_graphs}   # return is a dictionary contain input and target for this batch
        return feed_dict

    @staticmethod
    def _create_predict_feed_dict(input_graph, input_ph):
        # Graphs with no edges may make the network crash.  Filter them before proceeding.
        assert len(input_graph['edges']) == len(input_graph['senders']) == len(input_graph['receivers'])
        assert len(input_graph['edges']) > 0

        # Censor ground truth for input graphs.
        input_graph['globals'] = [0.] * len(ac.actions)
        input_graphs = gn.utils_np.data_dicts_to_graphs_tuple([input_graph])

        feed_dict = {input_ph: input_graphs}
        return feed_dict

    @staticmethod
    def _compute_accuracy(target, output) -> Tuple[np.ndarray, np.ndarray]:
        tdds = gn.utils_np.graphs_tuple_to_data_dicts(target)
        odds = gn.utils_np.graphs_tuple_to_data_dicts(output)
        correct = []  # Prediction was 100% correct.
        somewhat_correct = []  # Ground truth is in top 3 predictions.
        for td, od in zip(tdds, odds):
            xl = np.argmax(td['globals'], axis=-1)
            out_cp = np.copy(od['globals'])
            yl1 = np.argmax(out_cp, axis=-1)  # Top guess.
            out_cp[yl1] = -100000  # Make small to get second biggest value.
            yl2 = np.argmax(out_cp, axis=-1)  # Second best guess.
            out_cp[yl2] = -100000  # Make small to get second biggest value.
            yl3 = np.argmax(out_cp, axis=-1)  # Third best guess.

            if xl == yl1:
                correct.append(1.)
            else:
                correct.append(0.)

            if xl in [yl1, yl2, yl3]:
                somewhat_correct.append(1.)
            else:
                somewhat_correct.append(0.)

        return np.mean(correct), np.mean(somewhat_correct)

    @staticmethod
    def _compute_confusions(target, output) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
        tdds = gn.utils_np.graphs_tuple_to_data_dicts(target)
        odds = gn.utils_np.graphs_tuple_to_data_dicts(output)
        ground_truth = np.zeros([len(tdds)], dtype=np.int64)
        top_1 = np.zeros([len(tdds)], dtype=np.int64)
        top_3 = np.zeros([len(tdds)], dtype=np.int64)
        for i, (td, od) in enumerate(zip(tdds, odds)):
            xl = np.argmax(td['globals'], axis=-1)   # find the groundtruth action index
            out_cp = np.copy(od['globals'])
            yl1 = np.argmax(out_cp, axis=-1)  # Top guess.
            out_cp[yl1] = -100000  # Make small to get second biggest value.
            yl2 = np.argmax(out_cp, axis=-1)  # Second best guess.
            out_cp[yl2] = -100000  # Make small to get third biggest value.
            yl3 = np.argmax(out_cp, axis=-1)  # Third best guess.
            yl = xl if xl in [yl1, yl2, yl3] else yl1
            ground_truth[i] = xl
            top_1[i] = yl1  # predicted best action value
            top_3[i] = yl   # the groundtruth if it is in top3 prediction
        assert len(ground_truth) == len(top_1) == len(top_3)
        return ground_truth, top_1, top_3


# Construct a multilayerperceptron
def make_mlp_model(layer_count: int, neuron_count):
    """
    Instantiates a new MLP, followed by LayerNorm.

    The parameters of each new MLP are not shared with others generated by this function.

    :param layer_count: Amount of layers.
    :param neuron_count: Amount of neurons per layer.
    :return: A lambda returning a Sonnet module which contains the MLP and LayerNorm.
    """

    return lambda: snt.Sequential([
        snt.nets.MLP([neuron_count] * layer_count, activate_final=True),
        snt.LayerNorm() # This is a generic implementation of normalization along specific axes of the input.
    ])


class MLPGraphIndependent(snt.AbstractModule):
    """ GraphIndependent with MLP edge, node, and global models. """

    def __init__(self, layer_count: int, neuron_count: int, name='MLPGraphIndependent'):
        super(MLPGraphIndependent, self).__init__(name=name)
        with self._enter_variable_scope():
            self._network = gn.modules.GraphIndependent(
                edge_model_fn=make_mlp_model(layer_count, neuron_count), # here the functions are multilayer perceptron followed by layer normalization
                node_model_fn=make_mlp_model(layer_count, neuron_count),
                global_model_fn=make_mlp_model(layer_count, neuron_count))

    def _build(self, inputs):
        return self._network(inputs)


class MLPGraphNetwork(snt.AbstractModule):
    """ GraphNetwork with MLP edge, node, and global models. """

    def __init__(self, layer_count: int, neuron_count: int, name='MLPGraphNetwork'):
        super(MLPGraphNetwork, self).__init__(name=name)
        with self._enter_variable_scope():
            self._network = gn.modules.GraphNetwork(
                edge_model_fn=make_mlp_model(layer_count, neuron_count),
                node_model_fn=make_mlp_model(layer_count, neuron_count),
                global_model_fn=make_mlp_model(layer_count, neuron_count))

    def _build(self, inputs):
        return self._network(inputs)


class EncodeProcessDecode(snt.AbstractModule):
    """ Full encode-process-decode model.

    The model we explore includes three components:
    - An "Encoder" graph net, which independently encodes the edge, node, and
      global attributes (does not compute relations etc.).
    - A "Core" graph net, which performs N rounds of processing (message-passing)
      steps. The input to the Core is the concatenation of the Encoder's output
      and the previous output of the Core (labeled "Hidden(t)" below, where "t" is
      the processing step).
    - A "Decoder" graph net, which independently decodes the edge, node, and
      global attributes (does not compute relations etc.), on each message-passing
      step.

                        Hidden(t)   Hidden(t+1)
                           |            ^
              *---------*  |  *------*  |  *---------*
              |         |  |  |      |  |  |         |
    Input --->| Encoder |  *->| Core |--*->| Decoder |---> Output(t)
              |         |---->|      |     |         |
              *---------*     *------*     *---------*
    """

    def __init__(self, layer_count: int = 2, neuron_count: int = 32,
                 edge_output_size: int = None, node_output_size: int = None, global_output_size: int = None,
                 name='EncodeProcessDecode'):
        super(EncodeProcessDecode, self).__init__(name=name)
        self.layer_count = layer_count
        self.neuron_count = neuron_count
        self._encoder = MLPGraphIndependent(layer_count, neuron_count)  # Graph independent
        self._core = MLPGraphNetwork(layer_count, neuron_count)         # Graph network
        self._decoder = MLPGraphIndependent(layer_count, neuron_count)  # Graph independent
        # Transforms the outputs into the appropriate shapes.
        if edge_output_size is None:
            edge_fn = None
        else:
            # snt.linear: a linear layer only defines the output dimensionality, and the module has a name
            edge_fn = lambda: snt.Linear(edge_output_size, name='edge_output')
        if node_output_size is None:
            node_fn = None
        else:
            node_fn = lambda: snt.Linear(node_output_size, name='node_output')
        if global_output_size is None:
            global_fn = None
        else:
            global_fn = lambda: snt.Linear(global_output_size, name='global_output')
        with self._enter_variable_scope():
            self._output_transform = gn.modules.GraphIndependent(edge_fn, node_fn, global_fn)

    def _build(self, input_op, num_processing_steps):
        latent = self._encoder(input_op)
        latent0 = latent
        output_ops = []
        ac.print3('Building graph net with {}x{} neurons and {} processing steps.'.format(self.layer_count,
                                                                                          self.neuron_count,
                                                                                          num_processing_steps))
        for _ in range(num_processing_steps):
            core_input = gn.utils_tf.concat([latent0, latent], axis=1)
            latent = self._core(core_input)
            decoded_op = self._decoder(latent)
            output_ops.append(self._output_transform(decoded_op))
        return output_ops
