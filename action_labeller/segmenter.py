import cv2
import os
import time
import sys
import numpy
import json
import gc
import threading
import psutil
from typing import List

import bimacs


left = 0
right = 1


EXIT = 0
SEGMENT_NEXT = 1
SEGMENT_PREV = 2


class Recording:

    def __init__(self, path: str, segmentation_file_path: str, preload: bool = False):
        self.suppress_output = True

        # Detailed info about recording.
        self.subject_id: str = ''
        self.task_id: str = ''
        self.take_id: str = ''
        self.subject_id, self.task_id, self.take_id, _ = path[path.find('/subject_') + 1:].split('/')

        # Initialise members for recording data.
        self.path = path
        self.segmentation_file_path = segmentation_file_path
        self.rec_id = self.path[self.path.find('/subject_') + 1:].replace('/', ' | ')
        self.segmentation = [[], []]  # L/R segmentation points.
        self.frames = []
        self.frames_loaded = threading.Event()
        self.frames_loading = threading.Event()
        self.abort_loading_frames = False
        self.has_segmentation_json = False

        # Further initialise from additional data structures / sources
        self.recmeta = {}
        self.percent_loaded = 0
        self.load_metadata_contents()
        for side in [left, right]:
            self.segmentation[side] = [0, None, self.recmeta['frameCount'] - 1]
        self.load_segmentation()

        # Preload frames asynchronously if explicitly set
        if preload:
            self.preload_frames()

    def load_metadata_contents(self):
        with open(os.path.join(self.path, 'metadata.csv')) as metadata:
            for line in metadata:
                var_name, var_type, var_value = line.split(',')
                if var_name == 'name':
                    continue

                if var_type == 'unsigned int' or var_type == 'int':
                    var_value_typed = int(var_value)
                elif var_type == 'string':
                    var_value_typed = var_value.strip()
                else:
                    var_value_typed = var_value

                self.recmeta[var_name] = var_value_typed

    def load_segmentation(self):
        if os.path.isfile(self.segmentation_file_path):
            with open(self.segmentation_file_path) as segfile:
                segfile_json = json.load(segfile)
                # Left side of the segmenter is subjects right hand side and vice versa
                segmentation = {}
                segmentation[left] = segfile_json['right_hand']
                segmentation[right] = segfile_json['left_hand']

                self.assert_segmentation_not_malformatted(segmentation)

                self.segmentation[left] = segmentation[left]
                self.segmentation[right] = segmentation[right]
                self.has_segmentation_json = True

    def save_segmentation(self):
        self.assert_segmentation_not_malformatted(self.segmentation)
        with open(self.segmentation_file_path + '~', 'w') as segfile:
            segpretty = {}
            # Left side of the segmenter is subjects right hand side and vice versa
            segpretty['right_hand'] = self.segmentation[left]
            segpretty['left_hand'] = self.segmentation[right]
            json.dump(segpretty, segfile)
        os.rename(self.segmentation_file_path + '~', self.segmentation_file_path)

    def assert_segmentation_not_malformatted(self, segmentation):
        try:
            for side in [left, right]:
                assert len(segmentation[side]) % 2 == 1, 'Segmentation is expected to be of odd length'
                old_keypoint = -1
                for i in range(0, len(segmentation[side]), 1):
                    element = segmentation[side][i]
                    # Even: keypoint
                    if i % 2 == 0:
                        assert isinstance(element, int), 'Keypoint must be an integer'
                        assert element >= 0, 'Keypoint must be greater than or equal to zero'
                        assert element <= self.recmeta['frameCount'], 'Keypoint must be less than frame count'
                        assert old_keypoint < element, 'Prior keypoint is larger than or equal to current one'
                        old_keypoint = element
                    # Odd: action ID
                    else:
                        assert isinstance(element, int) or element is None, 'Action ID must be either integer or null/None'
                        if isinstance(element, int):
                            assert element >= 0, 'Action ID must be greater than or equal to zero'
                            assert element < len(bimacs.actions), 'Action ID must be less than actions count'
                        else:
                            assert element is None
        except Exception as e:
            print('Assertion failed in {}'.format(self.rec_id))
            raise e

    def preload_frames(self):
        self.frames_loading.set() # will be unset in preload_frames_sync eventually
        thread = threading.Thread(target=self.preload_frames_sync, args=())
        thread.daemon = True
        thread.start()

    def preload_frames_sync(self):
        self.frames_loading.set()

        # Shorthands
        frame_count = self.recmeta['frameCount']
        frames_per_chunk = self.recmeta['framesPerChunk']

        # Output printing
        print('Loading recording `{rec}` - {fps} fps and {fc} frames in total (chunk size: {fpc})'.format(rec=self.rec_id, fps=self.recmeta['fps'], fc=frame_count, fpc=frames_per_chunk))
        assert len(self.frames) == 0

        chunk_number = 0
        for current_frame in range(0, frame_count, 1):
            # Increment chunk number if appropriate
            if current_frame != 0 and current_frame % frames_per_chunk == 0:
                chunk_number = chunk_number + 1

            path_chunk = 'chunk_' + str(chunk_number)
            path_frame = 'frame_' + str(current_frame) + self.recmeta['extension']
            full_path_frame = os.path.join(self.path, path_chunk, path_frame)

            frame = None
            while frame is None:
                try:
                    frame = cv2.imread(full_path_frame)
                except:
                    print('{backspaces}Reading frame {f} failed, retrying...'.format(backspaces='\r' * 7, f=current_frame))
            self.frames.append(frame)
            self.percent_loaded = float(current_frame) / frame_count * 100

            # Check if loading was aborted for whatever reason
            if self.abort_loading_frames:
                break
        self.percent_loaded = 100

        if not self.abort_loading_frames:
            assert len(self.frames) == self.recmeta['frameCount']

        print('Loading recording `{rec}`: done'.format(rec=self.rec_id))

        self.frames_loaded.set()
        self.frames_loading.clear()

    def is_ready(self):
        return self.frames_loaded.is_set()

    def is_loading(self):
        return self.frames_loading.is_set()

    def wait_for_frames(self):
        while not self.frames_loaded.is_set():
            self.frames_loaded.wait(.25)

    def unload_frames(self):
        if not self.is_ready() and not self.is_loading():
            return

        if self.is_loading():
            self.abort_loading_frames = True
            self.wait_for_frames()
            self.abort_loading_frames = False
        self.frames_loaded.clear()
        del self.frames[:]
        self.frames = []
        gc.collect()


class LoadList:

    def __init__(self, recs: List[Recording]):
        self.max_size = 3
        self.l = []
        self.recs = recs
        self.num_max_length = len('{}'.format(len(self.recs) - 1))
        print('Initialised load list with a cache size of {} recordings'.format(self.max_size))

    def find_id(self, rec):
        return self.recs.index(rec)

    def enqueue_rec(self, rec):
        if rec in self.l:
            self.l.remove(rec)
        self.l.append(rec)

    def load_rec(self, rec):
        padded_num = '{}'.format(self.find_id(rec))
        while len(padded_num) < self.num_max_length:
            padded_num = ' ' + padded_num

        cache = 'Cache miss'
        if rec.is_ready():
            cache = 'Cache hit'
        #print('{backspaces} \033[32m<==\033[0m #{n} | Requested `{rec_id}` ({cache})'.format(backspaces='\r' * 7, n=padded_num, rec_id=rec.rec_id, cache=cache))

        # Preload frames if not already doing so
        if not rec.is_ready() and not rec.is_loading():
            rec.preload_frames()

        return rec.percent_loaded

    def preload_recs(self):
        # Issue loading of all other apps in the queue
        for a in self.l:
            if not a.is_loading() and not a.is_ready():
                padded_num = '{}'.format(self.find_id(a))
                #print('{backspaces} \033[32m<==\033[0m #{n} | Requested `{rec_id}`, loading in background'.format(backspaces='\r' * 7, n=padded_num, rec_id=a.rec_id))
                a.preload_frames()

    def unload_recs(self):
        while len(self.l) > self.max_size:
            padded_num = '{}'.format(self.find_id(self.l[0]))
            while len(padded_num) < self.num_max_length:
                padded_num = ' ' + padded_num
            if self.l[0].is_loading() or self.l[0].is_ready():
                #print('{backspaces} \033[31m==>\033[0m #{n} | Unloading `{rec_id}`'.format(backspaces='\r' * 7, n=padded_num, rec_id=self.l[0].rec_id))
                self.l[0].unload_frames()
            del self.l[0]


class SegmenterApp:

    def __init__(self, loadlist: LoadList, ui_scale, view_mode = False, record = False, highlight_unlabelled = True):
        self.loadlist = loadlist

        self.suppress_output = True
        self.record = record
        self.highlight_unlabelled = highlight_unlabelled
        if self.record and not os.path.exists('./recordings'):
            os.mkdir('./recordings')

        # Initialise members for UI state.
        self.ui_scale = ui_scale
        self.window_title = 'actseg'
        self.browsing = False
        self.browsing_x = 0
        self.current_side = left
        self.current_action = 0
        self.view_mode = view_mode
        self.last_frame = None

        # Further initialise from additional data structures / sources
        self.metrics = {}

    def init_metrics(self, ui_scale, recording: Recording):
        self.metrics['scale'] = ui_scale
        self.metrics['frame_height'] = int(recording.recmeta['frameHeight'] * self.metrics['scale'])
        self.metrics['frame_width'] = int(recording.recmeta['frameWidth'] * self.metrics['scale'])
        self.metrics['trackbar_margin'] = int(10 * self.metrics['scale'])
        self.metrics['trackbar_height'] = int(50 * self.metrics['scale'])
        self.metrics['trackbar_width'] = self.metrics['frame_width'] - (2 * self.metrics['trackbar_margin'])

    def run(self, index: int) -> int:
        recs = self.loadlist.recs

        def get_id(current, step):
            if step == 0:
                return current

            sign = 1 if step > 0 else -1

            ret = current + sign
            if ret >= len(recs):
                ret = 0
            elif ret < 0:
                ret = len(recs) - 1
            return get_id(ret, step + (-sign))

        j = len(recs) - 1
        assert get_id(j // 2, 0) == j // 2
        assert get_id(j, 1) == 0
        assert get_id(j - 1, 1) == j
        assert get_id(j - 2, 1) == j - 1
        assert get_id(0, -1) == j
        assert get_id(j, 2) == 1
        assert get_id(j // 2, 1) == j // 2 + 1

        # Loop over each rec and run them one-by-one.
        code = SEGMENT_NEXT
        i = index
        gui_window_initialised: bool = False
        while code != EXIT:
            rec = recs[i]

            # Enqueue relevant recs.
            prev_rec = recs[get_id(i, -1)]
            self.loadlist.enqueue_rec(prev_rec)
            next_rec = recs[get_id(i, 1)]
            self.loadlist.enqueue_rec(next_rec)
            self.loadlist.enqueue_rec(rec)

            # Unload recs.
            self.loadlist.unload_recs()

            # Load current rec synchronously.
            exit_loop: bool = False
            while not rec.is_ready():
                percent = self.loadlist.load_rec(rec)

                if gui_window_initialised:
                    time_delta_ms = 50
                    frame = self.last_frame.copy()
                    self.draw_loading_indicator(frame, percent)

                    # Draw loading frame.
                    cv2.imshow(self.window_title, frame)
                    if cv2.waitKey(time_delta_ms) & 0xFF == 27:
                        exit_loop = True
                        break
            if exit_loop:
                break
            self.loadlist.preload_recs()

            # Initialise GUI window initially.
            if not gui_window_initialised:
                cv2.startWindowThread()
                cv2.namedWindow(self.window_title, cv2.WINDOW_AUTOSIZE | cv2.WINDOW_KEEPRATIO | cv2.WINDOW_GUI_NORMAL)
                cv2.setMouseCallback(self.window_title, self.mouse_handler)
                gui_window_initialised = True

            # Run app loop.
            print('Displaying `{rec}` (#{id})'.format(rec=rec.rec_id, id=i))
            code = self.run_app_loop(rec)

            if rec and i == len(recs) - 1:
                break

            # Evaluate next rec.
            if code == SEGMENT_NEXT:
                i = get_id(i, 1)
            elif code == SEGMENT_PREV:
                i = get_id(i, -1)

        cv2.destroyWindow(self.window_title)
        return 0

    def run_app_loop(self, rec: Recording):
        self.init_metrics(self.ui_scale, rec)
        assert rec.is_ready(), 'App is not ready'
        assert len(rec.frames) == rec.recmeta['frameCount'], 'Frames are not loaded'

        # Shorthands
        fps = rec.recmeta['fps']
        frame_count = rec.recmeta['frameCount']
        scale = self.ui_scale
        frame_height = self.metrics['frame_height']
        frame_width = self.metrics['frame_width']
        trackbar_height = self.metrics['trackbar_height']

        max_loop_time = 1. / fps

        current_frame = 0
        start_time = time.time()

        self.browsing = False
        self.current_side = left
        pause = False
        loop_segment = False
        forward = True
        playback_speed = 1.
        video_writer = None

        while cv2.getWindowProperty(self.window_title, 1) >= 0:
            frame = rec.frames[int(current_frame)].copy()
            scaled_frame = cv2.resize(frame, (0,0), fx=scale, fy=scale)
            frame_draw_area = numpy.zeros((frame_height + trackbar_height, frame_width, 3), numpy.uint8)
            frame_draw_area[:scaled_frame.shape[0], :scaled_frame.shape[1]] = scaled_frame
            frame = frame_draw_area

            # Draw UI elements
            self.draw_segmentation_indicator(rec, frame, current_frame)
            if not self.view_mode:
                self.draw_playback_speed_indicator(frame, playback_speed, loop_segment)
                self.draw_side_indicator(rec, frame)
                self.draw_action_label_indicator(frame)
            self.draw_trackbar(rec, frame, current_frame)

            # Show frame
            cv2.imshow(self.window_title, frame)
            if self.record:
                if video_writer is None:
                    four_cc = cv2.VideoWriter_fourcc(*'XVID')
                    filename = os.path.join('./recordings', 'recording_{}.avi'.format(
                        rec.rec_id[:rec.rec_id.find(' | recording')].replace(' | ', '_')))
                    video_writer = cv2.VideoWriter(filename, four_cc, fps, (frame.shape[1], frame.shape[0]))
                video_writer.write(frame)

            # Timings
            time_elapsed = time.time() - start_time
            time_delta = max_loop_time - time_elapsed

            # Wait
            time_delta_ms = int(time_delta * 1000)
            # If time is negative, the player couldn't keep up for whatever reason. Skip the missed frames in this case
            if not self.record:
                while time_delta_ms <= 0:
                    time_delta_ms = time_delta_ms + int(max_loop_time * 1000) # Proceed one timestap
                    current_frame = self.get_next_frame(rec, current_frame, playback_speed, pause, forward, loop_segment)
            if time_delta_ms <= 0:
                time_delta_ms = 1
            k = cv2.waitKey(time_delta_ms) & 0xFF

            # Timings
            start_time = time.time()

            # Read controls
            if k == ord(' '):
                pause = not pause
            elif k == 83: # arrow key left
                self.browsing = False
                pause = True
                current_frame = float(int(current_frame) + 1)
                if current_frame >= frame_count:
                    current_frame = frame_count - 1
            elif k == 81: # arrow key right
                self.browsing = False
                pause = True
                current_frame = float(int(current_frame) - 1)
                if current_frame < 0:
                    current_frame = 0
            elif k == 227 or k == 228: # ctrl r or l, to fast forward to next segment and stop
                found_segment = False
                for i in range(0, len(rec.segmentation[self.current_side]) - 1, 2):
                    segment_start = rec.segmentation[self.current_side][i]
                    if segment_start > current_frame:
                        browsing = False
                        current_frame = segment_start
                        pause = True
                        found_segment = True
                        break
                if not found_segment:
                    current_frame = 0
            elif k == ord('l'): # loop current segment toggle
                loop_segment = not loop_segment
            elif k == ord('r'): # forward/reverse toggle
                forward = not forward
            elif k == ord('+') or k == ord('e'):
                playback_speed = playback_speed + 0.25
                if playback_speed >= 3:
                    playback_speed = 3
            elif k == ord('-') or k == ord('q'):
                playback_speed = playback_speed - 0.25
                if playback_speed < 0.25:
                    playback_speed = 0.25
            elif k == ord('x'):
                self.current_action = 0
                self.current_side = left if self.current_side == right else right
            elif k == ord('a'):
                self.current_action = self.current_action - 1
                if self.current_action == -1:
                    self.current_action = len(bimacs.actions) - 1
            elif k == ord('d'):
                self.current_action = self.current_action + 1
                if self.current_action == len(bimacs.actions):
                    self.current_action = 0
            elif 48 <= k and k <= 57:
                self.current_action = k - 48
            elif k == ord('w'):
                oldlen = len(rec.segmentation[self.current_side])
                assert oldlen % 2 == 1
                if int(current_frame) not in self.current_keypoints(rec):
                    for i in range(0, len(rec.segmentation[self.current_side]) - 1, 2):
                        begin = rec.segmentation[self.current_side][i]
                        action = rec.segmentation[self.current_side][i + 1]
                        end = rec.segmentation[self.current_side][i + 2]
                        if begin < current_frame and current_frame < end:
                            rec.segmentation[self.current_side][i + 1] = None
                            rec.segmentation[self.current_side].insert(i + 1, action)
                            rec.segmentation[self.current_side].insert(i + 2, int(current_frame))
                            break
                newlen = len(rec.segmentation[self.current_side])
                assert newlen == oldlen or newlen == oldlen + 2
            elif k == ord('s'):
                oldlen = len(rec.segmentation[self.current_side])
                assert oldlen % 2 == 1
                i = self.get_closest_keypoint(rec, current_frame)
                if i is not None:
                    keyframe = rec.segmentation[self.current_side][i]
                    prev_action = rec.segmentation[self.current_side][i - 1]
                    next_action = rec.segmentation[self.current_side][i + 1]
                    if prev_action != next_action:
                        rec.segmentation[self.current_side][i - 1] = None
                    del rec.segmentation[self.current_side][i]
                    del rec.segmentation[self.current_side][i]
                newlen = len(rec.segmentation[self.current_side])
                assert newlen == oldlen or newlen == oldlen - 2
            elif k == ord('f'):
                for i in range(0, len(rec.segmentation[self.current_side]) - 1, 2):
                    begin = rec.segmentation[self.current_side][i]
                    end = rec.segmentation[self.current_side][i + 2]
                    if begin <= current_frame and current_frame < end:
                        rec.segmentation[self.current_side][i + 1] = self.current_action
            elif k == ord('c'):
                for i in range(0, len(rec.segmentation[self.current_side]) - 1, 2):
                    begin = rec.segmentation[self.current_side][i]
                    end = rec.segmentation[self.current_side][i + 2]
                    if begin <= current_frame and current_frame < end:
                        rec.segmentation[self.current_side][i + 1] = None
            elif k == ord('p') or k == ord('n'):
                self.last_frame = frame.copy()
                return SEGMENT_NEXT if k == ord('n') else SEGMENT_PREV
            elif k == 27:
                break

            # Determine next frame depending on mode (browsing, pause, playback)
            old_current_frame = current_frame
            current_frame = self.get_next_frame(rec, current_frame, playback_speed, pause, forward, loop_segment)

            if self.record and (current_frame < old_current_frame):
                video_writer.release()
                return SEGMENT_NEXT

            # Write to file
            if not self.view_mode:
                if k in [ord('w'), ord('s'), ord('f'), ord('c')]:
                    if not self.suppress_output:
                        sanitised = [rec.segmentation[self.current_side][i] if i % 2 == 0 else bimacs.actions[rec.segmentation[self.current_side][i]] for i in range(0, len(rec.segmentation[self.current_side]), 1)]
                        print('Segmentation: {}'.format(sanitised))
                    rec.save_segmentation()

        return EXIT

    def mouse_handler(self, event, x, y, flags, param):
        if not self.browsing and ((event == cv2.EVENT_LBUTTONDOWN and y > self.metrics['frame_height']) or event == cv2.EVENT_RBUTTONDOWN):
            self.browsing_x = x
            self.browsing = True
        elif self.browsing and event == cv2.EVENT_MOUSEMOVE:
            self.browsing_x = x
        elif self.browsing and (event == cv2.EVENT_LBUTTONUP or event == cv2.EVENT_RBUTTONDOWN):
            self.browsing = False

    def draw_segmentation_indicator(self, rec, frame, current_frame):
        frame_width = self.metrics['frame_width']
        frame_height = self.metrics['frame_height']
        margin = self.metrics['trackbar_margin']
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 1
        colour = None
        red = (0, 0, 200) # bgr
        blue = (200, 0, 0) # bgr
        white = (255, 255, 255)
        thickness = 2

        # Keyframe indicators
        is_left_keypoint = self.get_closest_keypoint(rec, current_frame, left) != None
        is_right_keypoint = self.get_closest_keypoint(rec, current_frame, right) != None
        if is_left_keypoint or is_right_keypoint:
            indicator_overlay = frame.copy()
            tenth = frame_width // 10
            if is_left_keypoint:
                indicator_overlay[:,:tenth] = (0, 0, 255)
            if is_right_keypoint:
                indicator_overlay[:,frame_width - tenth:] = (255, 0, 0)
            alpha = 0.66
            cv2.addWeighted(frame, alpha, indicator_overlay, 1 - alpha, 0, frame)

        # Segment indicators
        for side in [left, right]:
            for i in range(0, len(rec.segmentation[side]) - 1, 2):
                begin = rec.segmentation[side][i]
                action_id = rec.segmentation[side][i + 1]
                if action_id == None:
                    if self.highlight_unlabelled:
                        action = '[UNLABELLED]'
                    else:
                        continue
                else:
                    action = bimacs.actions[action_id]
                end = rec.segmentation[side][i + 2]

                if begin <= current_frame and current_frame < end:
                    retval, _ = cv2.getTextSize(action, font, font_scale, thickness)
                    height = (frame_height // 2) + (retval[1] // 2)
                    if side == left:
                        text_baseline_start = (margin, height)
                        colour = red
                    else:
                        text_baseline_start = (frame_width - retval[0] - margin, height)
                        colour = blue
                    cv2.putText(frame, action, text_baseline_start, font, font_scale, white, thickness * 4, cv2.LINE_AA)
                    cv2.putText(frame, action, text_baseline_start, font, font_scale, colour, thickness, cv2.LINE_AA)

    def draw_playback_speed_indicator(self, frame, playback_speed, loop_segment):
        text_baseline_start = (20, 40)
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 1
        colour = (23, 23, 23)
        white = (255, 255, 255)
        thickness = 3
        text = 'x{:.2f} ({})'.format(playback_speed, 'LS' if loop_segment else 'L')
        cv2.putText(frame, text, text_baseline_start, font, font_scale, white, thickness*4, cv2.LINE_AA)
        cv2.putText(frame, text, text_baseline_start, font, font_scale, colour, thickness, cv2.LINE_AA)

    def draw_side_indicator(self, rec, frame):
        scale = self.metrics['scale']
        frame_width = self.metrics['frame_width']
        frame_height = self.metrics['frame_height']

        twentieth = frame_width // 20
        radius = twentieth - (twentieth // 4)
        red = (0, 0, 255)
        blue = (255, 0, 0)
        white = (255, 255, 255)
        colour = {}
        colour[left] = red
        colour[right] = blue
        circle_thickness = int(3 * scale)
        mid = {}
        mid[left] = (twentieth, frame_height - twentieth)
        mid[right] = ((frame_width - (frame_width // 10)) + twentieth, frame_height - twentieth)

        # Circle
        cv2.circle(frame, mid[self.current_side], radius, white, circle_thickness + int(2 * scale), cv2.LINE_AA)
        cv2.circle(frame, mid[self.current_side], radius, colour[self.current_side], circle_thickness, cv2.LINE_AA)

        # Number of unlabelled segments
        for side in [left, right]:
            unlabelled_segments = len([x for x in rec.segmentation[side] if x is None])
            if unlabelled_segments > 0:
                font = cv2.FONT_HERSHEY_SIMPLEX
                font_scale = 1
                thickness = 2
                text = '{}'.format(unlabelled_segments)
                retval, _ = cv2.getTextSize(text, font, font_scale, thickness)
                text_baseline_start = (mid[side][0] - (retval[0] // 2), mid[side][1] + (retval[1] // 2))
                cv2.putText(frame, text, text_baseline_start, font, font_scale, white, thickness*4, cv2.LINE_AA)
                cv2.putText(frame, text, text_baseline_start, font, font_scale, colour[side], thickness, cv2.LINE_AA)

    def draw_action_label_indicator(self, frame):
        frame_width = self.metrics['frame_width']
        frame_height = self.metrics['frame_height']
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 0.5
        colour = (23, 23, 23)
        white = (255, 255, 255)
        thickness = 1
        text = 'sel. action: ' + bimacs.actions[self.current_action] + ' (' + str(self.current_action) + ')'
        retval, _ = cv2.getTextSize(text, font, font_scale, thickness)
        text_baseline_start = ((frame_width // 2) - (retval[0] // 2), frame_height - retval[1])
        cv2.putText(frame, text, text_baseline_start, font, font_scale, white, thickness*4, cv2.LINE_AA)
        cv2.putText(frame, text, text_baseline_start, font, font_scale, colour, thickness, cv2.LINE_AA)

    def draw_trackbar(self, rec, frame, current_frame):
        # Shorthands
        scale = self.metrics['scale']
        frame_count = rec.recmeta['frameCount']
        frame_width = self.metrics['frame_width']
        frame_height = self.metrics['frame_height']
        trackbar_width = self.metrics['trackbar_width']
        trackbar_height = self.metrics['trackbar_height']
        trackbar_margin = self.metrics['trackbar_margin']

        # Baseline of the trackbar elements
        baseline = frame_height + int(trackbar_height / 2)
        # Default thickness
        thickness = int(1 * scale)
        # Colour
        white = (255, 255, 255)
        red = (0, 0, 255)
        red_bright = (128, 128, 255)
        blue = (255, 0, 0)
        blue_bright = (255, 128, 128)
        grey = (22, 22, 22)
        # Radius for the pgoress indicator
        radius = int(5 * scale)
        # Current segment
        unlabelled_segments = []
        current_segment = None

        # Grey background
        cv2.rectangle(frame, (0, frame_height), (frame_width, frame_height + trackbar_height), grey, -1)

        def frame_to_x(frame_number):
            advance_rel = float(frame_number) / (frame_count - 1)
            advance = trackbar_margin + int(trackbar_width * advance_rel)
            return advance

        for i in range(0, len(rec.segmentation[self.current_side]) - 1, 2):
            begin = rec.segmentation[self.current_side][i]
            action = rec.segmentation[self.current_side][i + 1]
            end = rec.segmentation[self.current_side][i + 2]

            if begin <= current_frame and current_frame < end:
                current_segment = (begin, end)

            if action == None:
                unlabelled_segments.append((begin, end))

        # Segmentation dashes
        dash_extend = int(7 * scale)
        for side in [left, right]:
            for i in range(2, len(rec.segmentation[side]) - 1, 2):
                keyframe = rec.segmentation[side][i]
                colour = red if side == left else blue
                if self.get_closest_keypoint(rec, current_frame, side) == i:
                    colour = red_bright if side == left else blue_bright
                advance = frame_to_x(keyframe)
                rectangle_width = int(4 * scale)
                rectangle_height = int(10 * scale)
                triangle_height = int(8 * scale)
                colour_dep_sign = -1 if side == left else 1

                # Key points
                rectangle_edge_tl = (advance - (rectangle_width // 2), baseline + (colour_dep_sign * (triangle_height + rectangle_height)))
                rectangle_edge_bl = (advance - (rectangle_width // 2), baseline + (colour_dep_sign * triangle_height))
                rectangle_edge_br = (advance + (rectangle_width // 2), baseline + (colour_dep_sign * triangle_height))
                triangle_tip = (advance, baseline)
                triangle_points = numpy.array([triangle_tip, rectangle_edge_bl, rectangle_edge_br])

                cv2.rectangle(frame, rectangle_edge_tl, rectangle_edge_br, colour, -1)
                cv2.fillConvexPoly(frame, triangle_points, colour, cv2.LINE_AA)

        # Trackbar base line
        trackbar_start = (trackbar_margin, baseline)
        trackbar_end = (trackbar_margin + trackbar_width, baseline)
        cv2.line(frame, trackbar_start, trackbar_end, white, thickness)

        # Trackbar base line overlay if segment is unlabelled
        for segment in unlabelled_segments:
            unlabelled_segment_colour = (255, 0, 255)
            x_begin = frame_to_x(segment[0])
            x_end = frame_to_x(segment[1])
            cv2.line(frame, (x_begin, baseline), (x_end, baseline), unlabelled_segment_colour, thickness)

        if current_segment is not None:
            colour = (150, 150, 150)
            if current_segment in unlabelled_segments:
                colour = (255, 128, 255)
            cv2.line(frame, (frame_to_x(current_segment[0]), baseline), (frame_to_x(current_segment[1]), baseline), colour, thickness)

        # Circle indicating progress
        cv2.circle(frame, (frame_to_x(current_frame), baseline), radius, white, thickness, cv2.LINE_AA)

    def draw_loading_indicator(self, frame, percent: float):
        overlay = numpy.full(frame.shape, 255, numpy.uint8)
        alpha = 0.33
        cv2.addWeighted(frame, alpha, overlay, 1 - alpha, 0, frame)
        mid = (frame.shape[1] // 2, frame.shape[0] // 2)
        white = (255, 255, 255)
        black = (0, 0, 0)
        font = cv2.FONT_HERSHEY_SIMPLEX
        font_scale = 2
        thickness = 4
        text = 'loading... {0:.2f}%'.format(percent)
        retval, _ = cv2.getTextSize(text, font, font_scale, thickness)
        text_baseline_start = (mid[0] - (retval[0] // 2), mid[1] + (retval[1] // 2))
        cv2.putText(frame, text, text_baseline_start, font, font_scale, white, thickness * 4, cv2.LINE_AA)
        cv2.putText(frame, text, text_baseline_start, font, font_scale, black, thickness, cv2.LINE_AA)

    def get_next_frame(self, rec, current_frame, playback_speed, pause, forward, loop_segment):
        frame_count = rec.recmeta['frameCount']
        trackbar_margin = self.metrics['trackbar_margin']
        trackbar_width = self.metrics['trackbar_width']
        if self.browsing:
            clamped_browsing_x = max(trackbar_margin, min(self.browsing_x, trackbar_margin + trackbar_width))
            clamped_browsing_x = clamped_browsing_x - trackbar_margin
            seek_pos_rel = float(clamped_browsing_x) / trackbar_width
            current_frame = (frame_count - 1) * seek_pos_rel
        elif pause:
            pass
        else:
            first_frame = 0
            last_frame = frame_count - 1

            if loop_segment:
                for i in range(0, len(rec.segmentation[self.current_side]) - 1, 2):
                    begin = rec.segmentation[self.current_side][i]
                    end = rec.segmentation[self.current_side][i + 2]
                    if begin <= current_frame and current_frame < end:
                        first_frame = begin
                        last_frame = end - 1
                        break

            current_frame = current_frame + (playback_speed if forward else -playback_speed)

            if current_frame > last_frame:
                current_frame = first_frame
            elif current_frame < first_frame:
                current_frame = last_frame
        return current_frame

    def current_keypoints(self, rec: Recording, side = None):
        if side is None:
            side = self.current_side
        return [rec.segmentation[side][i] for i in range(0, len(rec.segmentation[side]), 2)]

    def get_closest_keypoint(self, rec: Recording, current_frame: float, side = None):
        max_frame = rec.recmeta['frameCount'] - 1
        current_frame = int(current_frame)
        if current_frame == 0 or current_frame == max_frame:
            return None
        if side is None:
            side = self.current_side
        # If the current frame is in the segmentation, return the corresponding index
        if current_frame in self.current_keypoints(rec, side):
            return rec.segmentation[side].index(current_frame)
        # Otherwise iterate over the keypoints and search for a match with a margin of 1, then 2, then 3
        for margin in [1, 2, 3]:
            for i in range(2, len(rec.segmentation[side]) - 1, 2):
                keyframe = rec.segmentation[side][i]
                if current_frame in range(keyframe - margin, keyframe + margin + 1, 1):
                    return i
        return None
