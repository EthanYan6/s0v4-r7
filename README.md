<div align="center">
<h1>s0v4-r7</h1>
<!-- 技术属性 -->
<img src="https://img.shields.io/badge/Firmware-Custom-2563eb?style=for-the-badge&logo=buffer&logoColor=white" /> <img src="https://img.shields.io/badge/radio-uvk5/uvk6-111827?style=for-the-badge&logo=radio&logoColor=white" /> <img src="https://img.shields.io/badge/RF-BK4819-1f2937?style=for-the-badge" /> <img src="https://img.shields.io/badge/Memory-64K-374151?style=for-the-badge" /> <img src="https://img.shields.io/badge/UI-Moto_R7_Style-0f172a?style=for-the-badge" /></br>
<!-- GitHub 数据 -->
<img src="https://img.shields.io/github/stars/EthanYan6/s0v4-r7?style=for-the-badge&color=2563eb" /> <img src="https://img.shields.io/github/downloads/EthanYan6/s0v4-r7/total?style=for-the-badge&color=2563eb" /> <img src="https://img.shields.io/github/v/release/EthanYan6/s0v4-r7?style=for-the-badge&color=2563eb" /> <img src="https://img.shields.io/github/license/EthanYan6/s0v4-r7?style=for-the-badge&color=2563eb" />
</div>

## 描述

> 基于俄罗斯fagci的s0v4进行的二次开发

1. 将1VOF页面修改为仿摩托罗拉R7的页面
2. 将菜单中选项 `1VOR` 修改为 `1 Moto R7`
3. 该页面下禁用了长按2进入Pro页面的逻辑
4. 顶部菜单栏。从左向右依次是信号强度（根据RSSI的强度等比例缩小至5格信号），直频（直频的时候显示|->|），功率（UL/L/M/H），亚音（-/CT/DC/-D），芯片（BK/BC/SI），最右侧是电池信息（可以从菜单中设置显示图标，会更美观）
> UL表示很低，M表示中，H表示高功率
> 
> -：无亚音（None）；CT：CTCSS 模拟亚音；DC：DCS 数字亚音；-D：反向 DCS（-DCS）
> 
> BK：BK4819（K6自带芯片）；BC：BK1080（K6收音机芯片）；SI：SI4732（改装收音机芯片）
5. 中间部分。正常情况，左侧边框黑色方框，接收信号时左侧边框是白色方框。第一行显示信道号+信道名，如果是频率模式显示VFO；第二行显示频率。右上角显示接收灵敏度。
6. 底部按钮，左侧为Menu，右侧显示当前频率的模式AM/FM/WFM等

## 示意图

<img width="936" height="599" alt="image" src="https://github.com/user-attachments/assets/b8069405-9323-4e4f-9f25-f80563e18887" />

<img width="1095" height="700" alt="image" src="https://github.com/user-attachments/assets/512419b9-e39d-4571-b4e3-cd9559361f88" />

<img width="1105" height="668" alt="image" src="https://github.com/user-attachments/assets/7331696a-6b69-4e23-a3a4-479544e76a7b" />


## 构建

```sh
make
```

## 刷机

```sh
k5prog -F -YYY -b ./bin/firmware.bin
```

