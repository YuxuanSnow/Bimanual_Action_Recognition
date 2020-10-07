import math

import numpy as np

import matplotlib
matplotlib.use('Agg')  # Must be called before importing plt
import matplotlib.pyplot as plt

import sklearn
import sklearn.metrics
import sklearn.utils.multiclass


def latexify(fig_width: float = None, fig_height: float = None, columns: int = 1) -> None:
    """
    Set up matplotlib's RC params for LaTeX plotting.
    Call this before plotting a figure.

    Parameters
    ----------
    fig_width : float, optional, inches
    fig_height : float,  optional, inches
    columns : {1, 2}
    """

    # code adapted from http://www.scipy.org/Cookbook/Matplotlib/LaTeX_Examples

    # Width and max height in inches for IEEE journals taken from
    # computer.org/cms/Computer.org/Journal%20templates/transactions_art_guide.pdf

    assert columns in [1, 2]

    if fig_width is None:
        fig_width = 3.39 if columns == 1 else 6.9 # width in inches

    if fig_height is None:
        golden_mean = (math.sqrt(5) - 1.0) / 2.0  # Aesthetic ratio.
        fig_height = fig_width * golden_mean  # Height in inches.

    max_height_inches = 8.0
    if fig_height > max_height_inches:
        print('WARNING: fig_height too large: {} so will reduce to {} inches.'.format(fig_height, max_height_inches))
        fig_height = max_height_inches

    params = {'backend': 'ps',
              'text.latex.preamble': ['\\usepackage{gensymb}'],
              'axes.labelsize': 10, # fontsize for x and y labels (was 10)
              'axes.titlesize': 10,
              #'text.fontsize': 8, # was 10
              'legend.fontsize': 8, # was 10
              'xtick.labelsize': 6,
              'ytick.labelsize': 6,
              'text.usetex': True,
              'figure.figsize': [fig_width, fig_height],
              'font.family': 'serif'
    }

    matplotlib.rcParams.update(params)


def format_axes(ax):
    spine_color = 'gray'

    for spine in ['top', 'right']:
        ax.spines[spine].set_visible(False)

    for spine in ['left', 'bottom']:
        ax.spines[spine].set_color(spine_color)
        ax.spines[spine].set_linewidth(0.5)

    ax.xaxis.set_ticks_position('bottom')
    ax.yaxis.set_ticks_position('left')

    for axis in [ax.xaxis, ax.yaxis]:
        axis.set_tick_params(direction='out', color=spine_color)

    return ax


def confusion_matrix(y_true, y_pred, classes, filename, normalize=False, title=None, cmap=plt.cm.Blues,
                     axis_subject: str = 'label'):
    """
    This function prints and plots the confusion matrix.
    Normalization can be applied by setting `normalize=True`.
    """
    if title is None:
        if normalize:
            title = 'Normalized confusion matrix'
        else:
            title = 'Confusion matrix, without normalization'

    # Compute confusion matrix.
    cm = sklearn.metrics.confusion_matrix(y_true, y_pred)
    # Only use the labels that appear in the data.
    classes = classes[sklearn.utils.multiclass.unique_labels(y_true, y_pred)]
    if normalize:
        cm = cm.astype('float') / cm.sum(axis=1)[:, np.newaxis]

    fig, ax = plt.subplots()
    im = ax.imshow(cm, interpolation='nearest', cmap=cmap, vmin=0, vmax=1)
    ax.figure.colorbar(im, ax=ax)
    # We want to show all ticks...
    ax.set(xticks=np.arange(cm.shape[1]), yticks=np.arange(cm.shape[0]),
           # ... and label them with the respective list entries.
           xticklabels=classes, yticklabels=classes,
           title=title, ylabel='True {}'.format(axis_subject), xlabel='Predicted {}'.format(axis_subject))

    # Rotate the tick labels and set their alignment.
    plt.setp(ax.get_xticklabels(), rotation=45, ha='right', rotation_mode='anchor')

    # Loop over data dimensions and create text annotations.
    fmt = '.2f' if normalize else 'd'
    thresh = cm.max() / 2.
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            txt = format(cm[i, j], fmt)[1:]
            if txt == '.00':
                txt = ''
            ax.text(j, i, txt,
                    ha='center', va='center',
                    color='white' if cm[i, j] > thresh else 'black', fontsize=5)
    fig.tight_layout()

    written = False
    while not written:
        try:
            fig.savefig(filename)
        except Exception as e:
            print(e)
        else:
            written = True
            plt.close(fig)


def loss_graph(logged_iterations, losses_tr, losses_ge, filename):
    fig, ax = plt.subplots()
    x = np.array(logged_iterations)
    # Loss.
    y_tr = losses_tr
    y_ge = losses_ge
    ax.plot(x, y_tr, 'k', label='Training')
    ax.plot(x, y_ge, 'k--', label='Test/generalization')
    ax.set_title('Loss across training')
    ax.set_xlabel('Training iteration')
    ax.set_ylabel('Loss (softmax cross-entropy)')
    ax.legend()
    written = False
    while not written:
        try:
            fig.savefig(filename)
        except Exception as e:
            print(e)
        else:
            written = True
            plt.close(fig)


def action_probability(probabilities, action, side, recording, filename):
    fig, ax = plt.subplots()
    x = range(0, len(probabilities), 1)
    y = probabilities
    ax.plot(x, y, 'k', label='Certainty')
    ax.set_title('Certainty for action {} ({}) in {}'.format(action, side, recording))
    ax.set_xlabel('Frame number')
    ax.set_ylabel('Certainty')
    ax.legend()
    written = False
    while not written:
        try:
            fig.savefig(filename)
        except Exception as e:
            print(e)
        else:
            written = True
            plt.close(fig)
