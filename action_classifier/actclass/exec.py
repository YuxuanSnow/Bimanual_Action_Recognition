import argparse
import json
import os
import random
import sklearn.metrics
import socket
import traceback
from typing import List, Dict
import shutil

import numpy as np

import actclass as ac


STATUS_OK = 0
STATUS_ERROR = 1
STATUS_INTERRUPTED = 2
STATUS_UNHANDLED_EXCEPTION = 128


def mkevenv(args) -> int:
    assert os.path.exists(args.basepath), 'Basepath `{}` is not a valid path.'.format(args.basepath)
    namespace_path = os.path.join(args.basepath, args.namespace)
    if os.path.exists(namespace_path):
        shutil.rmtree(namespace_path)
    assert not os.path.exists(namespace_path), 'Namespace `{}` already exists in `{}`'.format(args.namespace, args.basepath)

    # Prepare data.
    env_config = args.__dict__
    del env_config['command']
    del env_config['verbosity']

    # Create namespace.
    os.makedirs(namespace_path, exist_ok=False)
    with open(os.path.join(namespace_path, 'env_config.json'), 'w') as f:
        json.dump(env_config, f, indent=4)

    return STATUS_OK


def dataset(args) -> int:
    assert os.path.exists(args.basepath), 'Basepath `{}` is not a valid path.'.format(args.basepath)

    dataset_config = 'h{}'.format(args.history_size)

    assert not os.path.exists(os.path.join(args.basepath, 'dataset_caches', dataset_config)), \
        'Dataset configuration `{}` already exists in `{}/dataset_caches`'.format(dataset_config, args.basepath)

    if not ac.dataset.symbolic_datset_exists(args.basepath):
        ac.print1("Symbolic dataset does not exist.  Generating now from --raw-dataset-path='{}'.".format(
            args.raw_dataset_path))
        ac.dataset.generate_symbolic_dataset(args.raw_dataset_path, args.basepath)
    ac.print1('Generating dataset.')
    ac.dataset.generate_dataset(args.basepath, dataset_config, args.history_size)

    return STATUS_OK


def train(args) -> int:

    assert os.path.exists(args.basepath), 'Basepath `{}` is not a valid path.'.format(args.basepath)
    assert os.path.exists(os.path.join(args.basepath, 'dataset_caches', args.dataset_config)), \
        'Dataset config `{}` does not exist in `{}/dataset_caches`'.format(args.dataset_config, args.basepath)
    assert args.evaluation_id in [1, 2, 3, 4, 5, 6], 'Invalid subject ID: {}.'.format(args.evaluation_id)

    namespace_path = os.path.join(args.basepath, args.namespace)
    fold_path = os.path.join(namespace_path, 'leave_out_{}'.format(args.evaluation_id))

    assert os.path.exists(namespace_path), 'Namespace `{}` does not exist.'.format(args.namespace)
    if args.restore is None:
        assert not os.path.exists(fold_path), 'An evaluation was already started for subject #{}.'.format(
            args.evaluation_id)

    # Setup logging.
    os.makedirs(fold_path, exist_ok=True)
    ac.logfile_path = fold_path
    ac.write_logfile = True
    ac.print1('Persistent logging enabled.')

    def is_ev_sub(fs: ac.dataset.SceneGraphProxy):
        return fs.subject == args.evaluation_id
    # if current subject id equals evaluation id

    def is_vld(fs: ac.dataset.SceneGraphProxy):
        return fs.take == args.validation_id
    # if current take id equals validation id

    ac.print1('Prepare data.')
    train_set = ac.dataset.load(args.basepath, args.dataset_config,
                                evaluation_mode=args.evaluation_mode,
                                filter_if=lambda x: is_ev_sub(x) or is_vld(x))
    valid_set = ac.dataset.load(args.basepath, args.dataset_config,
                                evaluation_mode=args.evaluation_mode,
                                filter_if=lambda x: is_ev_sub(x) or not is_vld(x))
    random.shuffle(train_set)
    random.shuffle(valid_set)

    # Ensure that the datasets are valid.
    ac.print3('Train/Validation sizes: {} vs. {}.'.format(len(train_set), len(valid_set)))
    assert len(train_set) > len(valid_set)
    assert all(x.subject != args.evaluation_id for x in train_set), 'Test data in train set!'
    assert all(x.take != args.validation_id for x in train_set), 'Validation data in train set!'
    ac.print3('All records in train set are not from subject #{} and are not take #{}'.format(
        args.evaluation_id, args.validation_id))
    assert all(x.subject != args.evaluation_id for x in valid_set), 'Test data in validation set!'
    assert all(x.take == args.validation_id for x in valid_set), 'Non-validation data in validation set!'
    ac.print3('All records in valid set are not from subject #{} and are take #{}.'.format(
        args.evaluation_id, args.validation_id))

    ac.print1('Importing model.')
    import actclass.model
    model = ac.model.ActionClassifierModel(fold_path, processing_steps_count=args.processing_steps_count,
                                           layer_count=args.layer_count, neuron_count=args.neuron_count)
    model.train(train_set, valid_set,
                restore=args.restore, max_iteration=args.max_iteration, log_interval=args.log_interval,
                save_interval=args.save_interval)

    return STATUS_OK


def predict(args) -> int:
    assert os.path.exists(args.basepath), 'Basepath `{}` is not a valid path.'.format(args.basepath)
    assert os.path.exists(os.path.join(args.basepath, 'dataset_caches', args.dataset_config)), \
        'Dataset config `{}` does not exist in `{}/dataset_caches`'.format(args.dataset_config, args.basepath)
    assert args.evaluation_id in [1, 2, 3, 4, 5, 6], 'Invalid subject ID: {}.'.format(args.evaluation_id)

    namespace_path = os.path.join(args.basepath, args.namespace)
    fold_path = os.path.join(namespace_path, 'leave_out_{}'.format(args.evaluation_id))

    assert os.path.exists(namespace_path), 'Namespace `{}` does not exist.'.format(args.namespace)
    assert os.path.exists(fold_path), 'Fold for subject {} was not trained yet.'.format(args.evaluation_id)

    ac.print1('Prepare data.')
    test_set = ac.dataset.load(args.basepath, args.dataset_config,
                               evaluation_mode=args.evaluation_mode,
                               filter_if=lambda x: x.subject != args.evaluation_id)
    test_set.sort(key=lambda x: (x.subject, x.task, x.take, x.side, x.frame))

    # Ensure that the dataset is valid.
    ac.print3('Test size: {}.'.format(len(test_set)))
    assert all(x.subject == args.evaluation_id for x in test_set)
    ac.print3('All records in test set are from subject #{}.'.format(args.evaluation_id))

    ac.print1('Importing model.')
    import actclass.model
    model = ac.model.ActionClassifierModel(fold_path, processing_steps_count=args.processing_steps_count,
                                           layer_count=args.layer_count, neuron_count=args.neuron_count)
    model.predict(test_set, args.restore)

    return STATUS_OK


def evaluate(args) -> int:
    assert os.path.exists(args.basepath), 'Basepath `{}` is not a valid path.'.format(args.basepath)
    assert os.path.exists(os.path.join(args.basepath, args.namespace)), \
        'Could not find namespace `{}`'.format(args.namespace)

    namespace = os.path.join(args.basepath, args.namespace)

    ac.plot.latexify(fig_height=2.8)

    for frame_number in [1, 2, 3, 4, 5, 6]:
        assert os.path.exists(os.path.join(namespace, 'leave_out_{}'.format(frame_number), 'predictions')), \
            'Predictions not complete!'

    ac.print1('Load symbolic dataset.')
    data = ac.dataset.load_symbolic(args.basepath)

    def to_task_str(task_id: str) -> str:
        tasks = ['task_1_k_cooking', 'task_2_k_cooking_with_bowls', 'task_3_k_pouring',
                 'task_4_k_wiping', 'task_5_k_cereals', 'task_6_w_hard_drive',
                 'task_7_w_free_hard_drive', 'task_8_w_hammering', 'task_9_w_sawing']
        return tasks[int(task_id[2:3]) - 1]

    ground_truth: List[int] = []
    correct_top1: List[int] = []
    correct_top3: List[int] = []
    correct_top1_count: int = 0
    correct_top3_count: int = 0

    subtotals = []
    classes = np.array(ac.actions)
    os.makedirs(os.path.join(namespace, 'evaluation_results'), exist_ok=True)

    seg = {}

    ac.print1('Evaluating now...')
    for evaluation_id in [1, 2, 3, 4, 5, 6]:
        subject_ground_truth: List[int] = []
        subject_correct_top1: List[int] = []
        subject_correct_top3: List[int] = []
        subject_correct_top1_count: int = 0
        subject_correct_top3_count: int = 0

        path = os.path.join(args.basepath, args.namespace, 'leave_out_{}'.format(evaluation_id), 'predictions')
        for file_name in os.listdir(path):
            with open(os.path.join(path, file_name)) as f:
                predictions = json.load(f)

            _, subject_id, task_id, take_id = file_name[:-len('.json')].split('_')
            subject_id = subject_id.replace('s', 'subject_')
            task_id = to_task_str(task_id)
            take_id = take_id.replace('tk', 'take_')

            if subject_id not in seg:
                seg[subject_id] = {}
            if task_id not in seg[subject_id]:
                seg[subject_id][task_id] = {}
            if take_id not in seg[subject_id][task_id]:
                seg[subject_id][task_id][take_id] = {
                    'top1': {'left': [], 'right': []},
                    'top3': {'left': [], 'right': []}
                }

            assert 'subject_{}'.format(evaluation_id) == subject_id

            assert len(predictions['right']) == len(predictions['left'])
            frame_count = len(predictions['right'])

            for side in ['left', 'right']:
                for frame_number in range(0, frame_count, 1):
                    f = np.exp(predictions[side][frame_number])
                    normalised_predictions = f / np.sum(f)
                    del f
                    top3_pred_indices = np.argsort(-normalised_predictions, axis=-1)[0:3]

                    ground_truth_action = getattr(data[subject_id][task_id][take_id],
                                                  'ground_truth_{}'.format(side))[frame_number]

                    # Segmentation
                    seg[subject_id][task_id][take_id]['top1'][side].append(int(top3_pred_indices[0]))
                    seg[subject_id][task_id][take_id]['top3'][side].append(int(ground_truth_action
                                                                           if ground_truth_action in top3_pred_indices
                                                                           else top3_pred_indices[0]))

                    if ground_truth_action is None:
                        continue

                    # Ground truth.
                    ground_truth.append(ground_truth_action)
                    subject_ground_truth.append(ground_truth_action)

                    # Correct top 1.
                    correct_top1.append(top3_pred_indices[0])
                    subject_correct_top1.append(top3_pred_indices[0])
                    if top3_pred_indices[0] == ground_truth_action:
                        correct_top1_count += 1
                        subject_correct_top1_count += 1

                    # Correct top 3.
                    if ground_truth_action in top3_pred_indices:
                        correct_top3.append(ground_truth_action)
                        subject_correct_top3.append(ground_truth_action)
                        correct_top3_count += 1
                        subject_correct_top3_count += 1
                    else:
                        correct_top3.append(top3_pred_indices[0])
                        subject_correct_top3.append(top3_pred_indices[0])

        assert len(subject_ground_truth) == len(subject_correct_top1) == len(subject_correct_top3)

        ground_truth_np = np.array(subject_ground_truth, dtype=np.int64)
        correct_top1_np = np.array(subject_correct_top1, dtype=np.int64)
        correct_top3_np = np.array(subject_correct_top3, dtype=np.int64)

        top1_filename = os.path.join(namespace, 'evaluation_results',
                                     'subject{}_confusion_matrix_top1.pdf'.format(evaluation_id))
        top3_filename = os.path.join(namespace, 'evaluation_results',
                                     'subject{}_confusion_matrix_top3.pdf'.format(evaluation_id))
        ac.plot.confusion_matrix(ground_truth_np, correct_top1_np, classes, top1_filename, normalize=True, title='',
                                 axis_subject='action')
        ac.plot.confusion_matrix(ground_truth_np, correct_top3_np, classes, top3_filename, normalize=True, title='',
                                 axis_subject='action')

        total_count = len(subject_ground_truth)
        subtotal_stats = {
            'correct_top1_count': subject_correct_top1_count,
            'correct_top3_count': subject_correct_top3_count,
            'total_count': total_count,
            'percent_top1': subject_correct_top1_count / total_count,
            'percent_top3': subject_correct_top3_count / total_count
        }
        subtotals.append(subtotal_stats)

    assert len(ground_truth) == len(correct_top1) == len(correct_top3)

    ground_truth_np = np.array(ground_truth, dtype=np.int64)
    correct_top1_np = np.array(correct_top1, dtype=np.int64)
    correct_top3_np = np.array(correct_top3, dtype=np.int64)

    top1_filename = os.path.join(namespace, 'evaluation_results', 'confusion_matrix_top1.pdf')
    top3_filename = os.path.join(namespace, 'evaluation_results', 'confusion_matrix_top3.pdf')
    ac.plot.confusion_matrix(ground_truth_np, correct_top1_np, classes, top1_filename, normalize=True, title='',
                                 axis_subject='action')
    ac.plot.confusion_matrix(ground_truth_np, correct_top3_np, classes, top3_filename, normalize=True, title='',
                                 axis_subject='action')

    total_count = len(ground_truth)
    stats = {
        'correct_top1_count': correct_top1_count,
        'correct_top3_count': correct_top3_count,
        'total_count': len(ground_truth),
        'percent_top1': correct_top1_count / total_count,
        'percent_top3': correct_top3_count / total_count,
        'report_top1': sklearn.metrics.classification_report(ground_truth_np, correct_top1_np,
                                                             target_names=ac.actions, output_dict=True),
        'report_top3': sklearn.metrics.classification_report(ground_truth_np, correct_top3_np,
                                                             target_names=ac.actions, output_dict=True),
        'subtotals': subtotals,
    }

    with open(os.path.join(namespace, 'evaluation_results', 'stats.json'), 'w') as f:
        json.dump(stats, f, indent=4)

    os.makedirs(os.path.join(namespace, 'evaluation_results', 'segmentations'), exist_ok=True)
    for subject_id in seg:
        for task_id in seg[subject_id]:
            for take_id in seg[subject_id][task_id]:
                for kind in ['top1', 'top3']:
                    segmentation = {'left_hand': [], 'right_hand': []}
                    seg_lr = seg[subject_id][task_id][take_id][kind]
                    assert 'left' in seg_lr and 'right' in seg_lr
                    assert len(seg_lr['left']) == len(seg_lr['right'])
                    for side, action_list in seg_lr.items():
                        segmentation['{}_hand'.format(side)] = ac.util.to_segmentation_file(action_list)
                    filename = '{}_{}_{}_{}.json'.format(subject_id, task_id, take_id, kind)
                    with open(os.path.join(namespace, 'evaluation_results', 'segmentations', filename), 'w') as f:
                        json.dump(segmentation, f)

    return STATUS_OK


def parse_args(argv: List[str], env: Dict[str, str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(prog='ac')
    # ArgumentParser: Object for parsing command line strings into Python objects
    # prog -- name of the program. (default: sys.argv[0])
    subparsers = parser.add_subparsers(dest='command')
    mkevenv_parser = subparsers.add_parser('mkevenv')
    train_parser = subparsers.add_parser('train')
    predict_parser = subparsers.add_parser('predict')
    dataset_parser = subparsers.add_parser('dataset')
    evaluate_parser = subparsers.add_parser('evaluate')

    # Common arguments.
    # All parsers need the basepath.
    for p in [mkevenv_parser, train_parser, predict_parser, dataset_parser, evaluate_parser]:
        p.add_argument('-b', '--basepath', type=str, required=env['basepath_default'] is None,
                       default=env['basepath_default'],
                       help='Basepath to the namespace folders')
        p.add_argument('--verbose', '-v', action='count', dest='verbosity', default=0,
                       help='Sets the verbosity')
    # Namespace argument.
    for p in [mkevenv_parser, train_parser, predict_parser, evaluate_parser]:
        p.add_argument('-n', '--namespace', type=str, required=True, default='env',
                       help='String identifier of the namespace')
    # Left out subject ID, restore point for model.
    for p in [train_parser, predict_parser]:
        p.add_argument('-e', '--evaluation-id', metavar='[1,2,3,4,5,6]', type=int, choices=[1, 2, 3, 4, 5, 6],
                       required=True,
                       help='Numerical identifier of the left-out evaluation subject')
        p.add_argument('--restore', type=int, required=(p == predict_parser),
                       help='Iteration number of the state to restore')

    # Make evaluation environment.
    mkevenv_parser.add_argument('--dataset-config', type=str, required=True, default='h10',
                                help='String identifier of the dataset configuration')
    mkevenv_parser.add_argument('--processing-steps-count', type=int, default=10,
                                help='Number of processing steps the graph network should perform.')
    mkevenv_parser.add_argument('--layer-count', type=int, default=2,
                                help='Number of layers used for the MLPs in the graph network.')
    mkevenv_parser.add_argument('--neuron-count', type=int, default=32,
                                help='Number of neurons per layer used for the MLPs in the graph network.')
    mkevenv_parser.add_argument('--validation-id', type=int, default=7,
                                help='ID of the takes to use as validation set')
    mkevenv_parser.add_argument('--evaluation-mode', type=str, choices=['normal', 'contact', 'centroids'],
                                required=True, help='Evaluation mode. (normal, contacts only, bb centroids.)')
    # Train.
    train_parser.add_argument('--max-iteration', type=int, default=3000,
                              help='Maximum iteration number before interrupting.  Negative values = unlimited')
    train_parser.add_argument('--log-interval', type=int, default=120,
                              help='Interval in seconds after which to perform a validation')
    train_parser.add_argument('--save-interval', type=int, default=100,
                              help='Number of iterations after which a model checkpoint is saved')
    # Dataset.
    dataset_parser.add_argument('--history-size', type=int, default=10, required=False,
                                help='Amount of scene graphs to be considered in the history for temporal edges')
    dataset_parser.add_argument('--raw-dataset-path', type=str, default=env['dataset_path_default'],required=False,
                                help='Path to the raw dataset')

    # Parse args.
    args = {}
    parsed_args = parser.parse_args(argv)

    # Find environment configuration if applicable.
    if parsed_args.command in ['train', 'predict']:
        with open(os.path.join(parsed_args.basepath, parsed_args.namespace, 'env_config.json')) as f:
            args = json.load(f)
    # Add/overwrite env config with current arguments.
    for key, value in parsed_args.__dict__.items():
        args[key] = value

    # Echo configuration.
    ac.verbosity = args['verbosity']
    for key, value in args.items():
        ac.print1('{k:<25s} {v:<10s}'.format(k='{}:'.format(key), v=str(value)))

    return argparse.Namespace(**args)


def main(argv: List[str]):
    ac.print0('Running on `{}`.'.format(socket.gethostname()))
    env = {
        'dataset_path_default': "/home/yuxuan/project/Bimanual_Action_Recognition/KIT_BIMACS_DATASET",
        'basepath_default': "/home/yuxuan/project/Bimanual_Action_Recognition/Processed"
        # cannot use the env variable, have no idea why it doesn't work

        # 'dataset_path_default': os.getenv('BIMACS_DATASET_PATH', None),
        # 'basepath_default': os.getenv('BIMACS_BASEPATH', None)
        # os.getenv() method in Python returns the value of the environment variable key if it exists otherwise returns
        # the default value. So people may have to add them into environment variable to clarify the path.
    }

    print("Data_raw_path:", env['dataset_path_default'])
    print("Data_base_path:", env['basepath_default'])

    args = parse_args(argv, env)
    # this args contain methods and arguments
    # argv: methods in the module: [mkevenv, train, predict, dataset, evaluate]
    # if argv is dataset, then parse_args(dataset, env)
    try:
        code = getattr(ac.exec, args.command)(args)
        # Get a named attribute from an object; getattr(x, 'y') is equivalent to x.y.
        # getattr(object, method)(argument of method): object calls the method with the given argument
        # More specifically: ac.exec.args.command(args)
    except KeyboardInterrupt:
        ac.print0('Interrupted by user.')
        code = STATUS_INTERRUPTED
    except Exception:
        ac.print0('Exception occured!', silent=True)
        ac.print0(traceback.format_exc(), silent=True)
        code = STATUS_UNHANDLED_EXCEPTION
    ac.print1('Exiting with code {}.'.format(code))
    ac.wait_for_logfile()
    return code
