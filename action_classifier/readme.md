# actclass

## Prerequisites

System dependencies are `python3` and `pip3`. Tested with Python 3.6.7.

Then, setup virtualenvwrapper:

- Run `pip3 install virtualenv`
- Add `~/.local/bin` to `$PATH`: `export PATH="$HOME/.local/bin:$PATH"`

## Setup

In the project root directory, run `virtualenv -p python3 venv` and activate it with `source venv/bin/activate`.
You can install the dependencies with `pip install -r requirements.txt`

Following environment variables are considered and can be set as well: 

```bash
# Path to the bimanual actions dataset.
export BIMACS_DATASET_PATH="/path/to/bimanual_actions_dataset"
# Path to the outputs (auxiliary datasets, training models, evaluation).
export BIMACS_BASEPATH="/path/to/evaluation"
```

## Usage

Get general help with `./ac -h`, or `./ac <subcommand> -h` for help with a specific subcommand.
Available subcommands are:

- `dataset`: Generate the dataset (symbolic and graphs)
- `mkevenv`: Create a parametrised evaluation environment for consecutive commands <br>
Possible required environment setting: ./ac mkevenv --namespace env --dataset-config h10 --evaluation-mode normal
- `train`: Train a model
- `predict`: Record the predictions of a model for the given evaluation subject
- `evaluate`: Run various evaluations and analysis
