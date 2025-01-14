mindspore.Tensor.choose
=======================

.. py:method:: mindspore.Tensor.choose(choices, mode='clip')

    根据原始Tensor数组和一个索引数组构造一个新的Tensor。

    参数：
        - **choices** (Union[tuple, list, Tensor]) - 索引选择数组。原始输入Tensor和 `choices` 的广播维度必须相同。如果 `choices` 本身是一个Tensor，则其最外层的维度（即，对应于第0维的维度）被用来定义 `choices` 数组。
        - **mode** ('raise', 'wrap', 'clip', optional) - 指定如何处理 `[0, n-1]` 外部的索引：

          - **raise** - 引发异常（默认）；
          - **wrap** - 原值映射为对n取余后的值；
          - **clip** - 大于n-1的值会被映射为n-1。该模式下禁用负数索引。

    返回：
        Tensor，合并后的结果。

    异常：
        - **ValueError** - 输入Tensor和任一 `choices` 无法广播。