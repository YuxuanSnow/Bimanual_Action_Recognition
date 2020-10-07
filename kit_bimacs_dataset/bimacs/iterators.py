import os
from typing import Generator, Dict, List, Tuple, Optional

import bimacs


def walk_bimanual_segmentation(segmentation: Dict[str, List]) -> Generator[Tuple[str, int, str, int], None, None]:
  for side in bimacs.sides:
    for start, action, end in walk_segmentation(segmentation[side]):
      yield side, start, action, end


def walk_segmentation(segmentation: List) -> Generator[Tuple[int, str, int], None, None]:
  for i in range(0, len(segmentation) - 1, 2):
    start: int = segmentation[i]
    action: str = bimacs.id_to_action(segmentation[i + 1])
    end: int = segmentation[i + 2] - 1
    yield start, action, end


def walk_dataset() -> Generator[Tuple[str, str, str], None, None]:
  for subject in bimacs.subjects:
    for task in bimacs.tasks:
      for take in bimacs.takes:
        yield subject, task, take


def walk_dataset_paths(base_path: Optional[str] = None,
                       mod_subject = lambda x: x,
                       mod_task = lambda x: x,
                       mod_take = lambda x: x,
                       filter = lambda subject, task, take: False) -> Generator[str, None, None]:
  if base_path is None:
    base_path = ''
  elif not base_path.startswith('/'):
    base_path = os.path.join(os.getenv('KIT_BIMACS_DATASET', ''), base_path)

  for subject, task, take in walk_dataset():
    if not filter(subject, task, take):
      yield os.path.join(base_path, mod_subject(subject), mod_task(task), mod_take(take))
