
 # introduce
 - 这是物凄转化器

 - 源码文件 mian.cpp

 - 芙兰朵露识别 dnn 为 ` Flandre.onx `
 # 如何生成
 ``` bash
cmake -S . -B build
cmake --build build
 ``` 
 # 注意
 - 电脑必须有OpenCV `OpenCV里必须包含这两个dll: opencv_videoio_ffmpeg*.dll opencv_world*.dll" `

 - 环境变量设置
 ``` bash
    setx OpenCV_DIR "自己的opencv的目录\build\x64\vc16\lib"
```
- 电脑里一定要有**FFmpeg 的可执行文件**
   否则生成的mp4将没有声音
   FFmpeg 官网 http://ffmpeg.org 
   下载之后配置环境变量:
``` bash
    setx /M Path "你的ffmpeg路径\bin"
```
`//其他版本的也可以是vc17等等`

# 如何使用
- 在cmake成功之后 打开`build/Debug/`
- 将要替换的目标图片改名 input.jpg   `一定是.jpg的格式`
- 点击turn.exe 可以得到`output.mp4`
