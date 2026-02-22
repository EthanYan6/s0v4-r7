# 🦉 s0v4-r7

What?!

s0v4 (or sova) is pronounced as сова in Russian. So, its an owl =)

This project is re-[reborn](https://github.com/fagci/uvk5-fagci-reborn) firmware for Quansheng UV-K5 radio, using FreeRTOS to make scan faster.

基于俄罗斯fagci的s0v4开发

1. 将1VOF页面修改为仿摩托罗拉R7的页面
2. 将菜单中选项 `1VOR` 修改为 `1 Moto R7`
3. 该页面下禁用了长按2进入Pro页面的逻辑
4. 顶部菜单栏。从左向右依次是信号强度（根据RSSI的强度等比例缩小至5格信号），直频（直频的时候显示|->|），功率（UL/L/M/H），亚音（-/CT/DC/-D），芯片（BK/BC/SI），最右侧是电池信息（可以从菜单中设置显示图标，会更美观）

> UL表示很低，M表示中，H表示高功率
> -：无亚音（None）；CT：CTCSS 模拟亚音；DC：DCS 数字亚音；-D：反向 DCS（-DCS）
> BK：BK4819（K6自带芯片）；BC：BK1080（K6收音机芯片）；SI：SI4732（改装收音机芯片）

5. 中间部分。正常情况，左侧边框黑色方框，接收信号时左侧边框是白色方框。第一行显示信道号+信道名，如果是频率模式显示VFO；第二行显示频率。右上角显示接收灵敏度。
6. 底部按钮，左侧为Menu，右侧显示当前频率的模式AM/FM/WFM等

## Building

```sh
make
```

## Flashing

```sh
k5prog -F -YYY -b ./bin/firmware.bin
```

