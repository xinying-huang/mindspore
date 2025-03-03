# Copyright 2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

import numpy as np
import pytest
from mindspore import Tensor
from mindspore.ops import functional as F
from mindspore.ops.operations import math_ops
from mindspore.ops.functional import vmap
from mindspore.common.api import ms_function
from mindspore.common.api import _pynative_executor


def np_all_close_with_loss(out, expect):
    """np_all_close_with_loss"""
    return np.allclose(out, expect, 0.00001, 0.00001, equal_nan=True)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("data_type", [np.float, np.double])
def test_stft_real_input(data_type):
    """
    Feature: STFT cpu kernel real data input.
    Description: test the rightness of STFT cpu kernel.
    Expectation: Success.
    """
    x_np = np.array([[1, 1, 1, 1, 1, 1], [1, 1, 1, 1, 1, 1]]).astype(data_type)
    x_ms = Tensor(x_np)
    out_ms = F.stft(x_ms, 4, center=False, onesided=True)
    expect = np.array([[[[4, 0], [4, 0], [4, 0]],
                        [[0, 0], [0, 0], [0, 0]],
                        [[0, 0], [0, 0], [0, 0]]],
                       [[[4, 0], [4, 0], [4, 0]],
                        [[0, 0], [0, 0], [0, 0]],
                        [[0, 0], [0, 0], [0, 0]]]]).astype(data_type)
    assert np.allclose(out_ms.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("input_data_type", [np.complex64, np.complex128])
@pytest.mark.parametrize("win_data_type", [np.complex64, np.complex128])
def test_stft_complex_input(input_data_type, win_data_type):
    """
    Feature: STFT cpu kernel complex data input.
    Description: test the rightness of STFT cpu kernel.
    Expectation: Success.
    """
    x_np = np.array([[1j, 1j, 1j, 1j, 1j, 1j], [1j, 1j, 1j, 1j, 1j, 1j]]).astype(input_data_type)
    win_np = np.array([1j, 1j, 1j, 1j]).astype(win_data_type)
    x_ms = Tensor(x_np)
    win_ms = Tensor(win_np)
    out_ms = F.stft(x_ms, 4, window=win_ms, center=False, onesided=False, return_complex=True)
    expect = np.array([[[-4, -4, -4],
                        [0, 0, 0],
                        [0, 0, 0],
                        [0, 0, 0]],
                       [[-4, -4, -4],
                        [0, 0, 0],
                        [0, 0, 0],
                        [0, 0, 0]]]).astype(np.complex128)
    assert np.allclose(out_ms.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("input_data_type", [np.complex64, np.complex128])
@pytest.mark.parametrize("win_data_type", [np.float, np.double])
def test_stft_diff_type(input_data_type, win_data_type):
    """
    Feature: STFT cpu kernel.
    Description: test the rightness of STFT cpu kernel.
    Expectation: Success.
    """
    x_np = np.array([[1 + 1j, 1 + 1j, 1 + 1j, 1 + 1j, 1 + 1j, 1 + 1j],
                     [1 + 1j, 1 + 1j, 1 + 1j, 1 + 1j, 1 + 1j, 1 + 1j]]).astype(input_data_type)
    win_np = np.array([1, 1, 1, 1]).astype(win_data_type)
    x_ms = Tensor(x_np)
    win_ms = Tensor(win_np)
    out_ms = F.stft(x_ms, 4, window=win_ms, center=False, onesided=False,
                    return_complex=True, normalized=True)
    expect = np.array([[[2 + 2j, 2 + 2j, 2 + 2j],
                        [0, 0, 0],
                        [0, 0, 0],
                        [0, 0, 0]],
                       [[2 + 2j, 2 + 2j, 2 + 2j],
                        [0, 0, 0],
                        [0, 0, 0],
                        [0, 0, 0]]]).astype(np.complex128)
    assert np.allclose(out_ms.asnumpy(), expect)


@pytest.mark.level0
@pytest.mark.platform_x86_cpu
@pytest.mark.env_onecard
@pytest.mark.parametrize("data_type", [np.float, np.double])
def test_stft_vmap(data_type):
    """
    Feature: STFT cpu kernel
    Description: test the rightness of STFT cpu kernel vmap feature.
    Expectation: Success.
    """

    def stft_fun(x, win):
        """stft func"""
        return math_ops.STFT(64, 16, 64, False, False, True)(x, win)

    x_np = np.random.randn(20, 5, 100).astype(data_type)
    win_np = np.random.randn(20, 64).astype(data_type)
    x = Tensor(x_np)
    win = Tensor(win_np)

    output_vmap = vmap(stft_fun, in_axes=(0, 0))(x, win)
    _pynative_executor.sync()

    @ms_function
    def manually_batched(xs, wins):
        """manually_batched"""
        output = []
        for i in range(xs.shape[0]):
            output.append(stft_fun(xs[i], wins[i]))
        return F.stack(output)

    output_manually = manually_batched(x, win)
    _pynative_executor.sync()

    assert np_all_close_with_loss(output_vmap.asnumpy(), output_manually.asnumpy())
