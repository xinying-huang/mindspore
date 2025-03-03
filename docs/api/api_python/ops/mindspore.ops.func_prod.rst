mindspore.ops.prod
==================

.. py:function:: mindspore.ops.prod(x, axis=(), keep_dims=False)

    默认情况下，通过计算指定维度中所有元素的乘积来移除该维度。也可仅缩小该维度大小至1。 `keep_dims` 控制输出和输入的维度是否相同。

    参数：
        - **x** (Tensor[Number]) - 输入Tensor。
          shape: :math:`(N,*)` ，其中 :math:`*` 表示任意数量的附加维度，其秩应小于8。
        - **axis** (Union[int, tuple(int), list(int)]) - 要减少的维度。默认值: ()，缩小所有维度。只允许常量值。取值必须在[-rank( `x` ), rank( `x` ))范围内。
        - **keep_dims** (bool) - 如果为True，则保留缩小的维度，大小为1。否则移除维度。默认值：False。

    返回：
        Tensor。

        - 如果 `axis` 为()，且 `keep_dims` 为False，则输出一个0维Tensor，表示输入Tensor中所有元素的乘积。
        - 如果 `axis` 为int，值为2，并且 `keep_dims` 为False，则输出Tensor的shape为 :math:`(x_1, x_3, ..., x_R)` 。
        - 如果 `axis` 为tuple(int)，值为(2, 3)，并且 `keep_dims` 为False，则输出Tensor的shape为 :math:`(x_1, x_4, ..., x_R)` 。

    异常：
        - **TypeError** - `x` 不是Tensor。
        - **TypeError** - `axis` 不是以下数据类型之一：int、Tuple或List。
        - **TypeError** - `keep_dims` 不是bool类型。
