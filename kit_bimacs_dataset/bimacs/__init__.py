#!/usr/bin/env python


from typing import List

from bimacs.convert_functions import action_to_id, id_to_action
from bimacs.iterators import walk_dataset, walk_dataset_paths, walk_segmentation, walk_bimanual_segmentation


# Dataset folders.
derived_data = 'bimacs_derived_data'
rgbd_data = 'bimacs_rgbd_data'
rgbd_data_ground_truth = 'bimacs_rgbd_data_ground_truth'

# Dataset subjects, tasks and takes.

# Totally 6 different people in dataset to help generalization
subject_1: str = 'subject_1'
subject_2: str = 'subject_2'
subject_3: str = 'subject_3'
subject_4: str = 'subject_4'
subject_5: str = 'subject_5'
subject_6: str = 'subject_6'
subjects: List[str] = [subject_1, subject_2, subject_3, subject_4, subject_5, subject_6]
# Totally 9 tasks of each people
task_1_cooking: str = 'task_1_k_cooking'
task_2_cooking_with_bowls: str = 'task_2_k_cooking_with_bowls'
task_3_pouring: str = 'task_3_k_pouring'
task_4_wiping: str = 'task_4_k_wiping'
task_5_cereals: str = 'task_5_k_cereals'
task_6_hard_drive: str = 'task_6_w_hard_drive'
task_7_free_hard_drive: str = 'task_7_w_free_hard_drive'
task_8_hammering: str = 'task_8_w_hammering'
task_9_sawing: str = 'task_9_w_sawing'
kitchen_tasks: List[str] = [task_1_cooking, task_2_cooking_with_bowls, task_3_pouring, task_4_wiping, task_5_cereals]
workshop_tasks: List[str] = [task_6_hard_drive, task_7_free_hard_drive, task_8_hammering, task_9_sawing]
tasks: List[str] =  kitchen_tasks + workshop_tasks
# Totally 9 takes of each tasks of each subject (9 Mal aufgenommen)
take_0: str = 'take_0'
take_1: str = 'take_1'
take_2: str = 'take_2'
take_3: str = 'take_3'
take_4: str = 'take_4'
take_5: str = 'take_5'
take_6: str = 'take_6'
take_7: str = 'take_7'
take_8: str = 'take_8'
take_9: str = 'take_9'
takes: List[str] = [take_0, take_1, take_2, take_3, take_4, take_5, take_6, take_7, take_8, take_9]
# Could be operated by left hand or right hand
left_hand_side: str = 'left_hand'
right_hand_side: str = 'right_hand'
sides: List[str] = [left_hand_side, right_hand_side]

# Dataset objects, relations and actions.
class object:
  bowl = 'bowl'
  knife = 'knife'
  screwdriver = 'screwdriver'
  cutting_board = 'cutting board'
  whisk = 'whisk'
  hammer = 'hammer'
  bottle = 'bottle'
  cup = 'cup'
  banana = 'banana'
  cereals = 'cereals'
  sponge = 'sponge'
  wood = 'wood'
  saw = 'saw'
  hard_drive = 'hard drive'
  left_hand = 'left hand'
  right_hand = 'right hand'
objects: List[str] = [object.bowl, object.knife, object.screwdriver, object.cutting_board, object.whisk, object.hammer,
                      object.bottle, object.cup, object.banana, object.cereals, object.sponge, object.wood, object.saw,
                      object.hard_drive, object.left_hand, object.right_hand]
class relation:
  contact = 'contact'
  above = 'above'
  below = 'below'
  left_of = 'left of'
  right_of = 'right of'
  behind_of = 'behind of'
  in_front_of = 'in front of'
  inside = 'inside'
  surround = 'surround'
  moving_together = 'moving together'
  halting_together = 'halting together'
  fixed_moving_together = 'fixed moving together'
  getting_close = 'getting close'
  moving_apart = 'moving apart'
  stable = 'stable'
  temporal = 'temporal'
relations: List[str] = [relation.contact, relation.above, relation.below, relation.left_of, relation.right_of,
                        relation.behind_of, relation.in_front_of, relation.inside, relation.surround,
                        relation.moving_together, relation.halting_together, relation.fixed_moving_together,
                        relation.getting_close, relation.moving_apart, relation.stable, relation.temporal]
class action:
  idle = 'idle'
  approach = 'approach'
  retreat = 'retreat'
  lift = 'lift'
  place = 'place'
  hold = 'hold'
  pour = 'pour'
  cut = 'cut'
  hammer = 'hammer'
  saw = 'saw'
  stir = 'stir'
  screw = 'screw'
  drink = 'drink'
  wipe = 'wipe'
  unknown = 'unknown'
enabling_actions = [action.approach, action.retreat, action.lift, action.place, action.hold]
supporting_actions = [action.hold]
manipulating_actions = [action.lift, action.place, action.pour, action.cut, action.hammer, action.saw, action.stir,
                        action.screw, action.drink, action.wipe]
actions: List[str] = [action.idle, action.approach, action.retreat, action.lift, action.place, action.hold, action.pour,
                      action.cut, action.hammer, action.saw, action.stir, action.screw, action.drink, action.wipe]
