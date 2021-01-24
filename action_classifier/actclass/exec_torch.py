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
