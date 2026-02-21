# 🦉 s0v4-r7

What?!

s0v4 (or sova) is pronounced as сова in Russian. So, its an owl =)

This project is re-[reborn](https://github.com/fagci/uvk5-fagci-reborn) firmware for Quansheng UV-K5 radio, using FreeRTOS to make scan faster.

基于俄罗斯fagci的s0v4开发

1. 将1VOF页面修改为仿摩托罗拉R7的页面
2. 将菜单中选项 `1VOR` 修改为 `1 Moto R7`
3. 该页面下禁用了长按2进入Pro页面的逻辑
4. 中间部分，从上到下依次为RSSI条和中间框。中间框第一行如果为频率模式，显示VFO；如果为信道模式显示信道号+信道名称。第二行内容为频率。第三行内容默认为listening...，如果按了PTT发射，显示TX...
5. 底部按钮，左侧为Menu，右侧显示当前频率的模式AM/FM/WFM等

## Building

```sh
make
```

## Flashing

```sh
k5prog -F -YYY -b ./bin/firmware.bin
```

