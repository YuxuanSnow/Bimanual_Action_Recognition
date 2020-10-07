import pytest

import actclass as ac


def test_to_segmentation_file_trivial():
    assert ac.util.to_segmentation_file([]) == []
    assert ac.util.to_segmentation_file([0]) == [0, 0, 1]


def test_to_segmentation_file_same():
    action_list = [5] * 20
    true = [0, 5, 20]
    seg = ac.util.to_segmentation_file(action_list)

    assert seg == true


def test_to_segmentation_file_alternating():
    action_list = [0, 1, 2, 3, 4, 5, 6]
    true = [0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7]
    seg = ac.util.to_segmentation_file(action_list)

    assert seg == true


def test_to_segmentation_file():
    action_list = [0, 0, 0, 0, 0, 0, 2, 2, 5, 5, 5, 5, 5, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0]
    true = [0, 0, 6, 2, 8, 5, 13, 3, 18, 0, len(action_list)]
    seg = ac.util.to_segmentation_file(action_list)

    assert seg == true
