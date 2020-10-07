import pytest

import actclass as ac


def recording_setup(frame_count=1):
    return pytest.mark.parametrize(
        'recording',
        [{
            'frame_count': frame_count
        }],
        indirect=True
    )


def scene_graph_setup(history_size=1):
    return pytest.mark.parametrize(
        'scene_graph',
        [{
            'history_size': history_size
        }],
        indirect=True
    )


def recording_factory(frame_count: int) -> ac.dataset.Recording:
    recording_frames = [
        # 1st frame.
        {
            'objects': [
                ac.dataset.Object(class_index=ac.objects.index('right_hand'), instance_name='right_hand_0'),
                ac.dataset.Object(class_index=ac.objects.index('left_hand'), instance_name='left_hand_1'),
                ac.dataset.Object(class_index=ac.objects.index('cup'), instance_name='cup_2')
            ],
            'relations': [
                ac.dataset.Relation(subject_index=0, object_index=2, relation_name='contact'),
                ac.dataset.Relation(subject_index=0, object_index=2, relation_name='moving together'),
                ac.dataset.Relation(subject_index=0, object_index=1, relation_name='left of'),
                ac.dataset.Relation(subject_index=1, object_index=2, relation_name='below')
            ],
            'right_action': ac.actions.index('hold'),
            'left_action': ac.actions.index('idle')
        },
        # 2nd frame.
        {
            'objects': [
                ac.dataset.Object(class_index=ac.objects.index('right_hand'), instance_name='right_hand_0'),
                ac.dataset.Object(class_index=ac.objects.index('cup'), instance_name='cup_2')
            ],
            'relations': [
                ac.dataset.Relation(subject_index=0, object_index=1, relation_name='contact'),
                ac.dataset.Relation(subject_index=0, object_index=1, relation_name='moving together')
            ],
            'right_action': ac.actions.index('place'),
            'left_action': ac.actions.index('idle')
        },
        # 3rd frame.
        {
            'objects': [
                ac.dataset.Object(class_index=ac.objects.index('right_hand'), instance_name='right_hand_0'),
                ac.dataset.Object(class_index=ac.objects.index('cup'), instance_name='cup_2'),
                ac.dataset.Object(class_index=ac.objects.index('left_hand'), instance_name='left_hand_3'),
                ac.dataset.Object(class_index=ac.objects.index('bowl'), instance_name='bowl_4')
            ],
            'relations': [
                ac.dataset.Relation(subject_index=0, object_index=1, relation_name='moving apart'),
                ac.dataset.Relation(subject_index=0, object_index=2, relation_name='left of'),
                ac.dataset.Relation(subject_index=2, object_index=1, relation_name='below'),
                ac.dataset.Relation(subject_index=2, object_index=3, relation_name='above'),
                ac.dataset.Relation(subject_index=2, object_index=3, relation_name='getting close')
            ],
            'right_action': ac.actions.index('retreat'),
            'left_action': ac.actions.index('approach')
        }
    ]

    rec = ac.dataset.Recording()
    rec.frame_count = frame_count
    rec.objects = []
    rec.relations = []
    rec.ground_truth_right = []
    rec.ground_truth_left = []

    for i in range(0, frame_count, 1):
        rec.objects.append(recording_frames[i]['objects'])
        rec.relations.append(recording_frames[i]['relations'])
        rec.ground_truth_right.append(recording_frames[i]['right_action'])
        rec.ground_truth_left.append(recording_frames[i]['left_action'])

    return rec


@pytest.fixture
def recording(request) -> ac.dataset.Recording:
    return recording_factory(request.param['frame_count'])


@pytest.fixture
def scene_graph(request) -> ac.dataset.SceneGraph:
    rec = recording_factory(3)
    sgl = rec.to_scene_graphs(2, request.param['history_size'])
    flattened_sg = ac.dataset.flatten_scene_graphs(sgl)
    return flattened_sg


class TestRecording:

    @staticmethod
    def is_sg0(sg):
        assert sg.right_action == ac.actions.index('hold')
        assert sg.left_action == ac.actions.index('idle')

        assert len(sg.nodes) == 3
        assert sg.nodes[0][:2] == (ac.objects.index('right_hand'), 'right_hand_0')[:2]
        assert sg.nodes[1][:2] == (ac.objects.index('left_hand'), 'left_hand_1')[:2]
        assert sg.nodes[2][:2] == (ac.objects.index('cup'), 'cup_2')[:2]

        assert ac.relations.index('contact') in sg.edges[(0, 2)]
        assert ac.relations.index('moving together') in sg.edges[(0, 2)]
        assert ac.relations.index('left of') in sg.edges[(0, 1)]
        assert ac.relations.index('below') in sg.edges[(1, 2)]

    @staticmethod
    def is_sg1(sg):
        assert sg.right_action == ac.actions.index('place')
        assert sg.left_action == ac.actions.index('idle')

        assert len(sg.nodes) == 2
        assert sg.nodes[0][:2] == (ac.objects.index('right_hand'), 'right_hand_0')[:2]
        assert sg.nodes[1][:2] == (ac.objects.index('cup'), 'cup_2')[:2]

        assert ac.relations.index('contact') in sg.edges[(0, 1)]
        assert ac.relations.index('moving together') in sg.edges[(0, 1)]

    @staticmethod
    def is_sg2(sg):
        assert sg.right_action == ac.actions.index('retreat')
        assert sg.left_action == ac.actions.index('approach')

        assert len(sg.nodes) == 4
        assert sg.nodes[0][:2] == (ac.objects.index('right_hand'), 'right_hand_0')[:2]
        assert sg.nodes[1][:2] == (ac.objects.index('cup'), 'cup_2')[:2]
        assert sg.nodes[2][:2] == (ac.objects.index('left_hand'), 'left_hand_3')[:2]
        assert sg.nodes[3][:2] == (ac.objects.index('bowl'), 'bowl_4')[:2]

        assert ac.relations.index('moving apart') in sg.edges[(0, 1)]
        assert ac.relations.index('left of') in sg.edges[(0, 2)]
        assert ac.relations.index('below') in sg.edges[(2, 1)]
        assert ac.relations.index('getting close') in sg.edges[(2, 3)]
        assert ac.relations.index('above') in sg.edges[(2, 3)]

    @staticmethod
    @recording_setup(frame_count=1)
    def test_check_integrity_ok_size1(recording):
        recording.check_integrity()

    @staticmethod
    @recording_setup(frame_count=3)
    def test_check_integrity_ok_size3(recording):
        recording.check_integrity()

    @staticmethod
    @recording_setup(frame_count=1)
    def test_check_integrity_class_id_negative(recording):
        recording.objects[0][0].class_index = -1
        with pytest.raises(AssertionError):
            recording.check_integrity()

    @staticmethod
    @recording_setup(frame_count=1)
    def test_check_integrity_class_id_too_high(recording):
        recording.objects[0][0].class_index = 100
        with pytest.raises(AssertionError):
            recording.check_integrity()

    @staticmethod
    @recording_setup(frame_count=1)
    def test_check_integrity_duplicate_instance_name(recording):
        recording.objects[0][1].instance_name = 'right_hand_0'  # Instance name of object[0][0].
        with pytest.raises(AssertionError):
            recording.check_integrity()

    @staticmethod
    @recording_setup(frame_count=1)
    def test_check_integrity_relation_name_unknown(recording):
        unknown_relation_name = '__UNKNOWN_RELATION_NAME__'
        assert unknown_relation_name not in ac.relations
        recording.relations[0][1].relation_name = unknown_relation_name
        with pytest.raises(AssertionError):
            recording.check_integrity()

    @recording_setup(frame_count=1)
    def test_to_scene_graph(self, recording):
        sg = recording.to_scene_graph(0)
        self.is_sg0(sg)

    @recording_setup(frame_count=1)
    def test_to_scene_graphs_single(self, recording):
        sg = recording.to_scene_graph(0)
        sgl = recording.to_scene_graphs(0, 10)

        assert sgl == [sg]
        self.is_sg0(sg)

    @staticmethod
    @recording_setup(frame_count=3)
    def test_to_scene_graphs_mulit_zero(recording):
        sgl = recording.to_scene_graphs(2, 0)

        assert len(sgl) == 0

    @recording_setup(frame_count=3)
    def test_to_scene_graphs_multi_one_start(self, recording):
        sgl = recording.to_scene_graphs(0, 1)
        assert len(sgl) == 1
        self.is_sg0(sgl[0])

    @recording_setup(frame_count=3)
    def test_to_scene_graphs_multi_one_middle(self, recording):
        sgl = recording.to_scene_graphs(1, 1)
        assert len(sgl) == 1
        self.is_sg1(sgl[0])

    @recording_setup(frame_count=3)
    def test_to_scene_graphs_multi_one_end(self, recording):
        sgl = recording.to_scene_graphs(2, 1)
        assert len(sgl) == 1
        self.is_sg2(sgl[0])

    @staticmethod
    @recording_setup(frame_count=3)
    def test_to_scene_graphs_structure(recording):
        sg0 = recording.to_scene_graph(0)
        sg1 = recording.to_scene_graph(1)
        sg2 = recording.to_scene_graph(2)
        sg_list = recording.to_scene_graphs(2, 10)

        assert len(sg_list) == 3
        assert sg0 == sg_list[0]
        assert sg1 == sg_list[1]
        assert sg2 == sg_list[2]

    @staticmethod
    @recording_setup(frame_count=3)
    def test_to_scene_graphs_full(recording):
        sg_list = recording.to_scene_graphs(2, 10)
        sg0 = sg_list[0]
        sg1 = sg_list[1]
        sg2 = sg_list[2]

        assert [sg0, sg1, sg2] == sg_list
        TestRecording.is_sg0(sg0)
        TestRecording.is_sg1(sg1)
        TestRecording.is_sg2(sg2)

    @staticmethod
    @recording_setup(frame_count=3)
    def test_to_scene_graphs_subset(recording):
        sg_list = recording.to_scene_graphs(2, 2)
        sg1 = sg_list[0]
        sg2 = sg_list[1]

        assert len(sg_list) == 2
        assert [sg1, sg2] == sg_list
        TestRecording.is_sg1(sg1)
        TestRecording.is_sg2(sg2)


class TestSceneGraph:

    @staticmethod
    @scene_graph_setup(history_size=1)
    def test_to_data_dict_trivial(scene_graph):
        data_dict = scene_graph.to_data_dict()

        assert len(data_dict['nodes']) == len(scene_graph.nodes)
        assert len(data_dict['edges']) == len(data_dict['senders']) == len(data_dict['receivers']) \
            == len(scene_graph.edges)

        assert data_dict['globals'] == ac.dataset.one_hot_encode(len(ac.actions), [ac.actions.index('retreat')])

        obs = len(ac.objects)
        nodes = data_dict['nodes']
        assert len(nodes) == 4
        assert nodes[0][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('right_hand')])
        assert nodes[1][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('cup')])
        assert nodes[2][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('left_hand')])
        assert nodes[3][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('bowl')])

        edges = data_dict['edges']
        assert len(edges) == 4
        assert edges[0] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('moving apart')])
        assert edges[1] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('left of')])
        assert edges[2] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('below')])
        assert edges[3] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('getting close'),
                                                                         ac.relations.index('above')])

        senders = data_dict['senders']
        receivers = data_dict['receivers']
        assert senders[0] == 0 and receivers[0] == 1
        assert senders[1] == 0 and receivers[1] == 2
        assert senders[2] == 2 and receivers[2] == 1
        assert senders[3] == 2 and receivers[3] == 3

    @staticmethod
    @scene_graph_setup(history_size=2)
    def test_to_data_dict_temporal(scene_graph):
        data_dict = scene_graph.to_data_dict()

        assert len(data_dict['nodes']) == len(scene_graph.nodes)
        assert len(data_dict['edges']) == len(data_dict['senders']) == len(data_dict['receivers']) \
            == len(scene_graph.edges)

        assert data_dict['globals'] == ac.dataset.one_hot_encode(len(ac.actions), [ac.actions.index('retreat')])

        obs = len(ac.objects)
        nodes = data_dict['nodes']
        assert len(nodes) == 6
        assert nodes[0][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('right_hand')])
        assert nodes[1][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('cup')])
        assert nodes[2][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('right_hand')])
        assert nodes[3][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('cup')])
        assert nodes[4][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('left_hand')])
        assert nodes[5][:obs] == ac.dataset.one_hot_encode(len(ac.objects), [ac.objects.index('bowl')])

        edges = data_dict['edges']
        assert len(edges) == 7
        assert edges[0] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('contact'),
                                                                         ac.relations.index('moving together')])
        assert edges[1] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('moving apart')])
        assert edges[2] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('left of')])
        assert edges[3] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('below')])
        assert edges[4] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('getting close'),
                                                                         ac.relations.index('above')])
        assert edges[5] == edges[6] == ac.dataset.one_hot_encode(len(ac.relations), [ac.relations.index('temporal')])

        senders = data_dict['senders']
        receivers = data_dict['receivers']
        assert senders[0] == 0 and receivers[0] == 1
        assert senders[1] == 2 and receivers[1] == 3
        assert senders[2] == 2 and receivers[2] == 4
        assert senders[3] == 4 and receivers[3] == 3
        assert senders[4] == 4 and receivers[4] == 5
        assert senders[5] == 2 and receivers[5] == 0  # right_hand_0 temporal.
        assert senders[6] == 3 and receivers[6] == 1  # cup_2 temporal.


@recording_setup(frame_count=1)
def test_flatten_scene_graphs_single(recording):
    sgl = recording.to_scene_graphs(0, 10)
    assert len(sgl) == 1

    flattened_sg = ac.dataset.flatten_scene_graphs(sgl)
    assert sgl[0] == flattened_sg
    for key in flattened_sg.edges:
        assert ac.relations.index('temporal') not in flattened_sg.edges[key]


@recording_setup(frame_count=2)
def test_flatten_scene_graphs_two(recording):
    sgl = recording.to_scene_graphs(1, 10)
    assert len(sgl) == 2
    flattened_sg = ac.dataset.flatten_scene_graphs(sgl)

    sum_temporal_edges: int = 0
    for (sender, receiver), edge in flattened_sg.edges.items():
        if ac.relations.index('temporal') in edge:
            # Temporal edges should not contain any spatial relations.
            assert len(edge) == 1
            # Instance names of nodes connected by temporal edges must be equal.
            assert flattened_sg.nodes[sender][1] == flattened_sg.nodes[receiver][1]
            sum_temporal_edges += 1
    # Two temporal edges should have been added.
    assert len(flattened_sg.edges) == len(sgl[0].edges) + len(sgl[1].edges) + 2
    assert sum_temporal_edges == 2


def test_one_hot_encode():
    assert ac.dataset.one_hot_encode(3, [1]) == [0., 1., 0.]
    assert ac.dataset.one_hot_encode(5, []) == [0., 0., 0., 0., 0.]
    assert ac.dataset.one_hot_encode(0, []) == []
    assert ac.dataset.one_hot_encode(4, list(range(4))) == [1., 1., 1., 1.]
    assert ac.dataset.one_hot_encode(6, [0, 2, 4]) == [1., 0., 1., 0., 1., 0.]


def test_one_hot_encode_wrong_usage():
    with pytest.raises(ValueError):
        ac.dataset.one_hot_encode(0, [1])
    with pytest.raises(ValueError):
        ac.dataset.one_hot_encode(2, [-1])
    with pytest.raises(ValueError):
        ac.dataset.one_hot_encode(5, [0, 3, -1])
    with pytest.raises(ValueError):
        ac.dataset.one_hot_encode(3, [1, 3])
    # Wrong types.
    with pytest.raises(ValueError):
        ac.dataset.one_hot_encode(2, [None])
    with pytest.raises(ValueError):
        ac.dataset.one_hot_encode(2, 2)
