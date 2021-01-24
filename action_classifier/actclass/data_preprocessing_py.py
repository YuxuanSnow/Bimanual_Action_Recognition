import os
import json
import pickle
import time
import traceback
from typing import List, Tuple, Dict, Generator, Any
import numpy as np

objects = ['bowl', 'knife', 'screwdriver', 'cuttingboard', 'whisk', 'hammer', 'bottle', 'cup',
           'banana', 'cereals', 'sponge', 'woodenwedge', 'saw', 'harddrive', 'left_hand',
           'right_hand']
relations = ['contact', 'above', 'below', 'left of', 'right of', 'behind of', 'in front of',
             'inside', 'surround', 'moving together', 'halting together', 'fixed moving together',
             'getting close', 'moving apart', 'stable', 'temporal']
actions = ['idle', 'approach', 'retreat', 'lift', 'place', 'hold', 'pour', 'cut', 'hammer', 'saw',
           'stir', 'screw', 'drink', 'wipe']


def load(basepath: str, dataset_config: str, evaluation_mode: str = 'normal', filter_if=lambda x: False):
    datasets_basepath = os.path.join(basepath, 'dataset_caches')
    paths = get_dataset_paths(datasets_basepath, dataset_config)
    # Instantiate all scene graph proxies.
    dataset = [SceneGraphProxy(p, evaluation_mode) for p in paths]
    # Filter scene graph proxies by given function.
    filtered_dataset: List[SceneGraphProxy] = [x for x in dataset if not filter_if(x)]
    return filtered_dataset


def get_dataset_paths(dataset_basepath: str, dataset_config: str) -> List[str]:
    paths: List[str] = []

    for subject, task, take in crawl_dataset():
        rec_basepath = os.path.join(dataset_basepath, dataset_config, subject, task, take)
        frame_count: int = len(os.listdir(rec_basepath)) // 2
        for i in range(frame_count):
            paths.append(os.path.join(rec_basepath, 'frame_{}_left.cache'.format(i)))
            paths.append(os.path.join(rec_basepath, 'frame_{}_right.cache'.format(i)))

    paths.sort()

    return paths


def get_raw_dataset_paths(derived_data_basepath, ground_truth_basepath: str) -> Tuple[List[str], List[str]]:
    derived_data_paths: List[str] = []
    ground_truth_paths: List[str] = []
    # list contains path to the derived and groundtruth data

    for subject, task, take in crawl_dataset():
        derived_data_paths.append(os.path.join(derived_data_basepath, subject, task, take))
        ground_truth_paths.append(os.path.join(ground_truth_basepath, subject, task, take + '.json'))

    # derived_data_path: a path to the dataset
    # derived_data_paths: a list contains paths to dataset, with specific subject, task, and take

    return derived_data_paths, ground_truth_paths


def crawl_dataset() -> Generator[Tuple[str, str, str], None, None]:
    for subject in ['subject_{}'.format(i) for i in range(1, 7)]:
        for task in ['task_1_k_cooking', 'task_2_k_cooking_with_bowls', 'task_3_k_pouring',
                     'task_4_k_wiping', 'task_5_k_cereals', 'task_6_w_hard_drive',
                     'task_7_w_free_hard_drive', 'task_8_w_hammering', 'task_9_w_sawing']:
            for take in ['take_{}'.format(take_i) for take_i in range(10)]:
                yield subject, task, take
                # yield: function will return a generator


class BoundingBox:

    def __init__(self, x0: float = 0, x1: float = 0, y0: float = 0, y1: float = 0, z0: float = 0, z1: float = 0,
                 serialised_bb: Dict[str, Any] = None):
        self.x0: float = x0
        self.x1: float = x1
        self.y0: float = y0
        self.y1: float = y1
        self.z0: float = z0
        self.z1: float = z1
        if serialised_bb is not None:
            for a in ['x0', 'x1', 'y0', 'y1', 'z0', 'z1']:
                setattr(self, a, serialised_bb[a])
        assert x1 >= x0, '{} < {}'.format(x1, x0)
        assert y1 >= y0, '{} < {}'.format(y1, y0)
        assert z1 >= z0, '{} < {}'.format(z1, z0)

    def to_float_tuple(self) -> Tuple[float, float, float, float, float, float]:
        return self.x0, self.x1, self.y0, self.y1, self.z0, self.z1


class Object:

    def __init__(self, certainty: float = 0, class_index: int = -1, class_name: str = '', instance_name: str = '',
                 bounding_box: BoundingBox = BoundingBox(), past_bounding_box: BoundingBox = BoundingBox(),
                 serialised_object: Dict[str, Any] = None):
        self.certainty: float = certainty
        self.class_index: int = class_index
        self.class_name: str = class_name
        self.instance_name: str = instance_name
        self.bounding_box: BoundingBox = bounding_box
        self.past_bounding_box: BoundingBox = past_bounding_box
        if serialised_object is not None:
            for a in ['certainty', 'class_index', 'class_name', 'instance_name']:
                setattr(self, a, serialised_object[a])
            for a in ['bounding_box', 'past_bounding_box']:
                setattr(self, a, BoundingBox(serialised_bb=serialised_object[a]))
            if self.class_name == 'RightHand':
                self.class_index = objects.index('right_hand')
            elif self.class_name == 'LeftHand':
                self.class_index = objects.index('left_hand')


class Relation:

    def __init__(self, subject_index: int = -1, object_index: int = -1, relation_name: str = '',
                 serialised_relation: Dict[str, Any] = None):
        self.subject_index: int = subject_index
        self.object_index: int = object_index
        self.relation_name: str = relation_name
        if serialised_relation is not None:
            for a in ['subject_index', 'object_index', 'relation_name']:
                setattr(self, a, serialised_relation[a])


class GroundTruth:

    def __init__(self, gtlist):
        self.gtlist = gtlist

    def __getitem__(self, k):
        for i in range(0, len(self.gtlist) - 1, 2):
            begin = self.gtlist[i]
            action_id = self.gtlist[i + 1]
            end = self.gtlist[i + 2]
            if begin <= k < end:
                return action_id
        if k == self.gtlist[-1]:
            return self.gtlist[-2]
        raise IndexError()

    def __len__(self):
        return self.gtlist[-1] + 1


def one_hot_encode(length: int, elements: List[int]) -> List[float]:
    # Definition one_hot_encoding: a one-hot is a group of bits among which the legal combinations of values are only
    # those with a single high (1) bit and all the others low (0).
    if not isinstance(length, int) or length < 0:
        raise ValueError('length must be an int <= 0')
    if not isinstance(elements, list):
        raise ValueError('elements must be a list')
    output = [0.] * length
    for element in elements:
        if not isinstance(element, int) or not (0 <= element < length):
            raise ValueError('All elements must be an int with: 0 <= element < length')
        output[element] = 1.
    return output


class SceneGraph:

    def __init__(self):
        self.right_action: int = -1
        self.left_action: int = -1
        self.nodes: Dict[int, Tuple[int, str, Tuple[float, float, float, float, float, float]]] = {}
        self.edges: Dict[Tuple[int, int], List[int]] = {}

    def __eq__(self, other):
        if isinstance(other, SceneGraph):
            return (self.right_action == other.right_action and self.left_action == other.left_action and
                    self.nodes == other.nodes and self.edges == other.edges)
        return False

    def check_integrity(self):
        assert self.right_action is None or (0 <= self.right_action < len(actions))
        assert self.left_action is None or (0 <= self.left_action < len(actions))
        for node_id in range(0, len(self.nodes), 1):
            assert node_id in self.nodes

    def to_data_dict(self, mirrored=False) -> Dict[str, List]:
        action_id: int = self.right_action
        if mirrored:
            action_id = self.left_action
        graph_globals: List[float] = one_hot_encode(len(actions), [] if action_id is None else [action_id])

        graph_nodes: List[List[float]] = []
        for node_id, (class_id, _, bb) in self.nodes.items():
            if mirrored:
                class_id = SceneGraph._mirror_hands(class_id)
            graph_nodes.append(one_hot_encode(len(objects), [class_id]) + list(bb))

        graph_edges: List[List[float]] = []
        graph_senders: List[int] = []
        graph_receivers: List[int] = []
        for (sender, receiver), relations in self.edges.items():
            if mirrored:
                relations = [SceneGraph._mirror_relations(r) for r in relations]
            graph_edges.append(one_hot_encode(len(relations), relations))
            graph_senders.append(sender)
            graph_receivers.append(receiver)

        return {
            'globals': graph_globals,
            'nodes': graph_nodes,
            'edges': graph_edges,
            'senders': graph_senders,
            'receivers': graph_receivers
        }

    @staticmethod
    def _mirror_hands(class_id: int) -> int:
        if objects[class_id] == 'right_hand':
            return objects.index('left_hand')
        if objects[class_id] == 'left_hand':
            return objects.index('right_hand')
        return class_id

    @staticmethod
    def _mirror_relations(relation_id: int) -> int:
        if relations[relation_id] == 'left of':
            return relations.index('right of')
        if relations[relation_id] == 'right of':
            return relations.index('left of')
        return relation_id


def flatten_scene_graphs(scene_graph_list: List[SceneGraph]) -> SceneGraph:
    temporal_sg = SceneGraph()

    # Maps the scene graph id and local node id to a global node id.
    global_node_id_map: Dict[Tuple[int, int], int] = {}

    # Ground truth of the temporal scene graph is the ground truth of the most recent scene graph (last in list).
    temporal_sg.right_action = scene_graph_list[-1].right_action
    temporal_sg.left_action = scene_graph_list[-1].left_action

    global_node_id: int = 0
    # Add the nodes to the temporal scene graph.  Populate global node id map alongside.
    for sg_id, sg in enumerate(scene_graph_list):
        for node_id, node in sg.nodes.items():
            key = (sg_id, node_id)
            global_node_id_map[key] = global_node_id
            temporal_sg.nodes[global_node_id] = node
            global_node_id += 1

    # Add the edges to the temporal scene graph.
    for sg_id, sg in enumerate(scene_graph_list):
        for (sender, receiver), relations in sg.edges.items():
            key = (global_node_id_map[sg_id, sender], global_node_id_map[sg_id, receiver])
            temporal_sg.edges[key] = relations

    # Add the edges for the temporal relations.
    for sg_id in range(1, len(scene_graph_list), 1):
        sg = scene_graph_list[sg_id]
        past_sg = scene_graph_list[sg_id - 1]
        for node_id, (_, node_name, _) in sg.nodes.items():
            for past_node_id, (_, past_node_name, _) in past_sg.nodes.items():
                if node_name == past_node_name:
                    # One temporal edge from an object node to the corresponding node one step in the past.
                    key = (global_node_id_map[sg_id, node_id], global_node_id_map[sg_id - 1, past_node_id])
                    temporal_sg.edges[key] = [relations.index('temporal')]

    return temporal_sg


class Recording:

    def __init__(self, derived_data_path: str = None, ground_truth_path: str = None):
        if derived_data_path is None or ground_truth_path is None:
            raise ValueError('derived_data_path and ground_truth_path must both be set.')

        self.frame_count: int = 0
        self.derived_data_path: str = derived_data_path
        self.ground_truth_path: str = ground_truth_path
        self.objects: List[List[Object]] = []
        self.relations: List[List[Relation]] = []
        self.ground_truth_left: GroundTruth
        self.ground_truth_right: GroundTruth

        self.frame_count = len(os.listdir(os.path.join(self.derived_data_path, 'spatial_relations')))
        self._load_objects()
        self._load_relations()
        self._load_ground_truth()

    def check_integrity(self) -> None:
        # Check sizes.
        assert len(self.objects) == self.frame_count
        assert len(self.relations) == self.frame_count
        assert len(self.ground_truth_right) == self.frame_count
        assert len(self.ground_truth_left) == self.frame_count

        for frame in range(0, self.frame_count, 1):
            # Check objects.
            taken_instance_names: List[str] = []
            for obj in self.objects[frame]:
                assert 0 <= obj.class_index < len(objects)
                assert obj.instance_name not in taken_instance_names
                taken_instance_names.append(obj.instance_name)
            del taken_instance_names

            # Check relations.
            for rel in self.relations[frame]:
                assert rel.subject_index is not None and rel.object_index is not None
                assert rel.subject_index != rel.object_index
                assert rel.relation_name in ac.relations

    def to_scene_graph(self, frame_number: int) -> SceneGraph:
        assert 0 <= frame_number < self.frame_count, 'Requested frame out of range'

        sg = SceneGraph()

        # Annotations (actions).
        sg.right_action = self.ground_truth_right[frame_number]
        sg.left_action = self.ground_truth_left[frame_number]

        # Nodes (objects).
        for node_id, obj in enumerate(self.objects[frame_number]):
            bb = obj.bounding_box.to_float_tuple()
            sg.nodes[node_id] = (obj.class_index, obj.instance_name, bb)

        # Edges (relations).
        for relation in self.relations[frame_number]:
            key = (relation.subject_index, relation.object_index)
            if key not in sg.edges:
                sg.edges[key] = []
            sg.edges[key].append(relations.index(relation.relation_name))

        return sg

    def to_scene_graphs(self, frame_number: int, history_size: int = 10) -> List[SceneGraph]:
        assert 0 <= frame_number < self.frame_count, 'Requested frame out of range'
        # for each frame i define a list of Scenegraph
        sgl: List[SceneGraph] = []
        # here history_size = 10 means 10 scenegraph has to be considered for temporal edge
        # if current frame less than 10, than take all frames before into list sgl
        for i in range(max(frame_number - history_size + 1, 0), frame_number + 1, 1):
            sgl.append(self.to_scene_graph(i))
        return sgl

    def _load_objects(self):
        assert len(self.objects) == 0
        for obs in self._load_json_series('3d_objects'):
            self.objects.append([Object(serialised_object=ob) for ob in obs])
        assert len(self.objects) == self.frame_count

    def _load_relations(self):
        assert len(self.relations) == 0
        for rels in self._load_json_series('spatial_relations'):
            self.relations.append([Relation(serialised_relation=rel) for rel in rels])
        assert len(self.relations) == self.frame_count

    def _load_ground_truth(self):
        with open(self.ground_truth_path) as f:
            gtjson = json.load(f)
            self.ground_truth_left = GroundTruth(gtjson['left_hand'])
            self.ground_truth_right = GroundTruth(gtjson['right_hand'])

    def _load_json_series(self, type):
        def get_path(index):
            return os.path.join(self.derived_data_path, type, 'frame_{}.json'.format(index))
        for i in range(0, self.frame_count, 1):
            loaded_successfully = False
            while not loaded_successfully:
                try:
                    with open(get_path(i)) as of:
                        # path example for _load_objects(): derived_data_path + "3d_objects" + frame_10
                        yield json.load(of)
                    loaded_successfully = True
                except IOError:
                    print("Error while loading")

        assert not os.path.isfile(get_path(self.frame_count))


class SceneGraphProxy:
    def __init__(self, path: str, mode: str):
        self.path: str = path
        self.subject: int = -1
        self.task: int = -1
        self.take: int = -1
        self.frame: int = -1
        self.side: str = 'none'
        self.mode: str = mode
        assert os.path.isfile(path), 'Not a valid path: {}'.format(path)
        for part in self.path.split('/'):
            if part.startswith('subject_'):
                pos = part.find('subject_') + len('subject_')
                self.subject = int(part[pos:pos + 1])
            elif part.startswith('task_'):
                pos = part.find('task_') + len('task_')
                self.task = int(part[pos:pos + 1])
            elif part.startswith('take_'):
                pos = part.find('take_') + len('take_')
                self.take = int(part[pos:pos + 1])
            elif part.startswith('frame_'):
                if '_left' in part:
                    self.side = 'left'
                elif '_right' in part:
                    self.side = 'right'
                self.frame = int(part[len('frame_'):-len('_{}.cache'.format(self.side))])
        assert all(p >= 0 for p in [self.subject, self.task, self.take, self.frame])
        assert self.side != 'none'

    def load(self):
        with open(self.path, 'rb') as f:
            try:
                return getattr(self, 'load_{}'.format(self.mode))(pickle.load(f))
            except pickle.UnpicklingError as e:
                time.sleep(0.1)
                try:
                    return getattr(self, 'load_{}'.format(self.mode))(pickle.load(f))
                except pickle.UnpicklingError:
                    raise Exception('Repeatedly failed to unpickle file.')

    def load_contact(self, graph):
        graph = self.load_normal(graph)
        for i in range(len(graph['edges'])):
            for j, rel in enumerate(relations):
                if rel != 'contact' and rel != 'temporal':
                    graph['edges'][i][j] = 0.  # Censor all relations except for contact and temporal.
                    assert graph['edges'][i][j] == 0.
        return graph

    def load_centroids(self, graph):
        for i in range(len(graph['edges'])):
            for j, rel in enumerate(relations):
                if rel != 'temporal':
                    graph['edges'][i][j] = 0.  # Censor all relations except for temporal.
                    assert graph['edges'][i][j] == 0.
        for i in range(len(graph['nodes'])):
            offset = len(objects)
            cx = graph['nodes'][i][offset + 1] - graph['nodes'][i][offset]
            cy = graph['nodes'][i][offset + 3] - graph['nodes'][i][offset + 2]
            cz = graph['nodes'][i][offset + 5] - graph['nodes'][i][offset + 4]
            # Cut off bounding box data and replace it with centroid.
            graph['nodes'][i] = graph['nodes'][i][:-6] + [cx, cy, cz]
            assert len(graph['nodes'][i]) == len(ac.objects) + 3
        return graph

    def load_normal(self, graph):
        for i in range(len(graph['nodes'])):
            graph['nodes'][i] = graph['nodes'][i][:-6]  # Cut off bounding box data.
            assert len(graph['nodes'][i]) == len(ac.objects)
        return graph


def load_symbolic(out_path):
    cachefile = os.path.join(out_path, 'dataset_caches', 'symbolic_dataset.cache')
    with open(cachefile, 'rb') as f:
        recs = pickle.load(f)
        ac.print3('Done loading from cachefile.')
    return recs


def symbolic_datset_exists(basepath: str) -> bool:
    cachefile = os.path.join(basepath, 'dataset_caches', 'symbolic_dataset.cache')
    return os.path.isfile(cachefile)


def generate_symbolic_dataset(dataset_path, basepath) -> None:

    derived_data = 'bimacs_derived_data'
    rgbd_data_ground_truth = 'bimacs_rgbd_data_ground_truth'

    os.makedirs(os.path.join(basepath, 'dataset_caches'), exist_ok=True)
    cachefile = os.path.join(basepath, 'dataset_caches', 'symbolic_dataset.cache')
    recs = {}

    derived_data_paths, ground_truth_paths = get_raw_dataset_paths(os.path.join(dataset_path, derived_data),
                                             os.path.join(dataset_path, rgbd_data_ground_truth))

    for derived_data_path, ground_truth_path in zip(derived_data_paths, ground_truth_paths):
        *_, subject, task, take = derived_data_path.split('/')
        ac.print1('Generating objects for {} | {} | {}.'.format(subject, task, take))
        if subject not in recs:
            recs[subject] = {}
        if task not in recs[subject]:
            recs[subject][task] = {}
        rec = Recording(derived_data_path, ground_truth_path)
        recs[subject][task][take] = rec

    # Write cache file.
    with open(cachefile, 'wb') as f:
        pickle.dump(recs, f)


def generate_dataset(basepath: str, config: str, history_size: int) -> None:
    recs = load_symbolic(basepath)
    cachepath = os.path.join(basepath, 'dataset_caches')

    def write_frame(s, ts, tk, fr, graphs_to_write):
        # Write cache file
        os.makedirs(os.path.join(cachepath, config, s, ts, tk), exist_ok=True)
        for side in ['left', 'right']:
            cachefile = os.path.join(cachepath, config, s, ts, tk, 'frame_{}_{}.cache'.format(fr, side))
            written = False
            while not written:
                try:
                    with open(cachefile, 'wb') as f:
                        pickle.dump(graphs_to_write[side], f)
                except IOError:
                    ac.print0('Error writing frame.  Retrying...')
                    time.sleep(0.25)
                else:
                    written = True

    for subject, task, take in crawl_dataset():
        recording: Recording = recs[subject][task][take]
        recording.check_integrity()
        for i in range(0, recording.frame_count, 1):
            sgl = recording.to_scene_graphs(i, history_size=history_size)
            scene_graph = flatten_scene_graphs(sgl)
            scene_graph.check_integrity()
            graphs = {
                'right': scene_graph.to_data_dict(mirrored=False),
                'left': scene_graph.to_data_dict(mirrored=True)
            }
            write_frame(subject, task, take, i, graphs)


def main():

    dataset_path = "/home/yuxuan/project/Bimanual_Action_Recognition/KIT_BIMACS_DATASET"
    base_path = "/home/yuxuan/project/Bimanual_Action_Recognition"

    # generate_symbolic_dataset(dataset_path, base_path)

    print("666")


if __name__ == "__main__":
    main()
