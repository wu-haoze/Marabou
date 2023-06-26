import torch
from torch import nn


class Transpose(nn.Module):
    def __init__(self, perm=None):
        self.dims = perm
        super().__init__()

    def forward(self, data: torch.Tensor):
        if not self.dims:
            dims = tuple(reversed(range(data.dim())))
        else:
            dims = self.dims
        if not (len(dims) == 2 and dims[0] == 1):
            transposed = data.permute(dims)
        else:
            transposed = data
        return transposed
