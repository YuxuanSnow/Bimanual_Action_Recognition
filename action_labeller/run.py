import bimacs
import segmenter
import os
import argparse
from typing import List


def main():
    rec_root_path = os.getenv('KIT_BIMACS_DATASET', '')
    if rec_root_path == '':
        raise Exception('KIT_BIMACS_DATASET unset!')

    parser = argparse.ArgumentParser()
    parser.add_argument('--ff', action='store_true')
    parser.add_argument('--index', default='0')
    parser.add_argument('--rec', action='store_true')
    parser.add_argument('--view', action='store_true')
    parser.add_argument('--hide-unlabelled', action='store_true')
    args = parser.parse_args()

    index = args.index
    if '|' in index:
        parts = [int(i) for i in index.split('|')]
        assert len(parts) == 3
        parts[0] -= 1
        parts[1] -= 1
        index = parts[0] * 90 + parts[1] * 10 + parts[2]
    else:
        index = int(index)
    ff_to_unsegmented = args.ff

    # Create app for each recording
    recordings = []
    is_any_rec_preloading = False
    for i, (subject, task, take) in enumerate(bimacs.walk_dataset()):
        data_path = os.path.join(rec_root_path, bimacs.rgbd_data, subject, task, take, 'rgb')
        if not os.path.exists(data_path):
            print('Warning!  Path `{}` does not exist.  Skipping.'.format(data_path))
            continue
        segmentation_file_path = os.path.join(rec_root_path, bimacs.rgbd_data_ground_truth, subject, task)
        os.makedirs(segmentation_file_path, exist_ok=True)
        segmentation_file_path_filename = os.path.join(segmentation_file_path, '{}.json'.format(take))
        recording = segmenter.Recording(data_path, segmentation_file_path_filename)
        if not is_any_rec_preloading:
            if (ff_to_unsegmented and not recording.has_segmentation_json) or (not ff_to_unsegmented and i == index):
                preload = True
                index = i
            else:
                preload = False
            if preload:
                recording.preload_frames()
                is_any_rec_preloading = True
        recordings.append(recording)

    rec = args.rec
    view: str = args.view
    highlight_unlabelled: bool = not args.hide_unlabelled

    loadlist = segmenter.LoadList(recordings)
    app = segmenter.SegmenterApp(loadlist, ui_scale=float(os.getenv('UI_SCALE', 1)), record=rec, view_mode=view,
                                 highlight_unlabelled=highlight_unlabelled)
    app.run(index)

    print('Exited normally.')


if __name__ == '__main__':
    main()
