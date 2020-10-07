from typing import List, Any

import actclass as ac


def to_segmentation_file(action_list: List[int]) -> List[Any]:
    segmentation_list = []

    if len(action_list) > 0:
        last_action_added = None
        for frame, action in enumerate(action_list):
            if action != last_action_added:
                segmentation_list.append(frame)
                segmentation_list.append(action)
                last_action_added = action
        segmentation_list.append(len(action_list))

    return segmentation_list


def segmentation_file_to_mm_hr_action(segmentation_list: List[int], max_mm: float = -1) -> List[Any]:
    hr_list = []

    if max_mm > 0:
        scale = max_mm / segmentation_list[-1]
    else:
        scale = 1.
    for i, p in enumerate(segmentation_list):
        if i % 2 == 0:
            hr_list.append(p * scale)
        else:
            hr_list.append(ac.actions[p])

    return hr_list
