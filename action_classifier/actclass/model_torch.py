import torch
import torch.nn.functional as F


# Link function
class LinkFunction(torch.nn.Module):

    def __init__(self, link_def, args):
        super(LinkFunction, self).__init__()
        self.l_definition = ''
        self.l_function = None
        self.learn_args = torch.nn.ParameterList([])
        self.learn_modules = torch.nn.ModuleList([])
        self.__set_link(link_def, args)

    def forward(self, edge_features):
        return self.l_function(edge_features)

    def __set_link(self, link_def, args):
        self.l_definition = link_def.lower()
        self.args = args

        self.l_function = {
            'graphconv':        self.l_graph_conv,
            'graphconvlstm':    self.l_graph_conv_lstm,
        }.get(self.l_definition, None)

        if self.l_function is None:
            print('WARNING!: Update Function has not been set correctly\n\tIncorrect definition ' + link_def)
            quit()

        init_parameters = {
            'graphconv':        self.init_graph_conv,
            'graphconvlstm':    self.init_graph_conv_lstm,
        }.get(self.l_definition, lambda x: (torch.nn.ParameterList([]), torch.nn.ModuleList([]), {}))

        init_parameters()

    def get_definition(self):
        return self.l_definition

    def get_args(self):
        return self.args

    # Definition of linking functions
    # GraphConv
    def l_graph_conv(self, edge_features):
        last_layer_output = edge_features
        for layer in self.learn_modules:
            last_layer_output = layer(last_layer_output)
        return last_layer_output[:, 0, :, :]

    def init_graph_conv(self):
        input_size = self.args['edge_feature_size']
        hidden_size = self.args['link_hidden_size']

        if self.args.get('link_relu', False):
            self.learn_modules.append(torch.nn.ReLU())
            self.learn_modules.append(torch.nn.Dropout())
        for _ in range(self.args['link_hidden_layers']-1):
            self.learn_modules.append(torch.nn.Conv2d(input_size, hidden_size, 1))
            self.learn_modules.append(torch.nn.ReLU())
            # self.learn_modules.append(torch.nn.Dropout())
            # self.learn_modules.append(torch.nn.BatchNorm2d(hidden_size))
            input_size = hidden_size

        self.learn_modules.append(torch.nn.Conv2d(input_size, 1, 1))
        # self.learn_modules.append(torch.nn.Sigmoid())

    # GraphConvLSTM
    def l_graph_conv_lstm(self, edge_features):
        last_layer_output = self.ConvLSTM(edge_features)

        for layer in self.learn_modules:
            last_layer_output = layer(last_layer_output)
        return last_layer_output[:, 0, :, :]

    def init_graph_conv_lstm(self):
        input_size = self.args['edge_feature_size']
        hidden_size = self.args['link_hidden_size']
        hidden_layers = self.args['link_hidden_layers']

        self.ConvLSTM = ConvLSTM.ConvLSTM(input_size, hidden_size, hidden_layers)
        self.learn_modules.append(torch.nn.Conv2d(hidden_size, 1, 1))
        self.learn_modules.append(torch.nn.Sigmoid())


# GNN model
class GNN_Model(torch.nn.Module):

    def __init__(self, model_args):
        super(GNN_Model, self).__init__()

        self.model_args = model_args.copy()

        if model_args['resize_feature_to_message_size']:
            # Resize large features into message size with Linear functions and xavier normalization
            self.edge_feature_resize = torch.nn.Linear(model_args['edge_feature_size'], model_args['message_size'])
            self.node_feature_resize = torch.nn.Linear(model_args['node_feature_size'], model_args['message_size'])
            torch.nn.init.xavier_normal(self.edge_feature_resize.weight)
            torch.nn.init.xavier_normal(self.node_feature_resize.weight)

            model_args['edge_feature_size'] = model_args['message_size']
            model_args['node_feature_size'] = model_args['message_size']

        self.link_fun = Model.LinkFunction('GraphConv', model_args) # if needs temporal operation, then 'GraphConvLSTM'
        self.sigmoid = torch.nn.Sigmoid()
        self.message_fun = Model.MessageFunction('linear_concat_relu', model_args) # concatenate messages from all neghbor nodes
        self.update_fun = Model.UpdateFunction('gru', model_args)
        self.readout_fun = Model.ReadoutFunction('fc', {'readout_input_size': model_args['node_feature_size'], 'output_classes': model_args['hoi_classes']})

        self.propagate_layers = model_args['propagate_layers']

        self._load_link_fun(model_args)