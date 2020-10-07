from typing import Optional

import bimacs


def id_to_action(id: Optional[int]) -> str:
  if id is None:
    return bimacs.action.unknown

  assert 0 <= id < len(bimacs.actions), 'ID must be: 0 <= ID < len(bimac.actions)'

  return bimacs.actions[id]


def action_to_id(action: str) -> Optional[int]:
  if action == bimacs.action.unknown:
    return None

  assert action in bimacs.actions, 'Action must be any of: {}'.format(bimacs.actions)

  return bimacs.actions.index(action)
