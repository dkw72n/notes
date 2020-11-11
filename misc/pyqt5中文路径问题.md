pyqt5 中文路径问题
=====

## 错误信息
```
qt.qpa.plugin: Could not find the Qt platform plugin "windows" in ""
This application failed to start because no Qt platform plugin could be initialized. Reinstalling the application may fix this problem.
```

## 原因

### pyqt5

#### qpy/QtCore/qpycore_qt_conf.cpp
```
// Embed a qt.conf file.
bool qpycore_qt_conf()
{
    //...
    // Check if there is a bundled copy of Qt.
    if (QFileInfo(qt_dir_name).exists())
    {
        // Get the prefix path with non-native separators.
        static QByteArray qt_conf = qt_dir_name.toLocal8Bit();

        qt_conf.prepend("[Paths]\nPrefix = ");
        qt_conf.append("\n");
        // ...
    }
    // ...
}
```
pyqt5 内嵌了一个 `qt.conf`, 里面写入的路径的使用的是当前编码 `Local8bit`, 简中环境通常就是 `gbk`.

### qt5

#### qtbase/src/corelib/io/qsettings.cpp
```
QSettingsPrivate::QSettingsPrivate(QSettings::Format format)
    : format(format), scope(QSettings::UserScope /* nothing better to put */), iniCodec(0), fallbacks(true),
      pendingChanges(false), status(QSettings::NoError)
{
}

// ...

bool QSettingsPrivate::iniUnescapedStringList(const QByteArray &str, int from, int to,
                                              QString &stringResult, QStringList &stringListResult,
                                              QTextCodec *codec)
{
// ...
#if !QT_CONFIG(textcodec)
            Q_UNUSED(codec)
#else
            if (codec) {
                stringResult += codec->toUnicode(str.constData() + i, j - i);
            } else
#endif
            {
                int n = stringResult.size();
                stringResult.resize(n + (j - i));
                QChar *resultData = stringResult.data() + n;
                for (int k = i; k < j; ++k)
                    *resultData++ = QLatin1Char(str.at(k));
            }
            i = j;
// ...
}
```
`QSettingsPrivate` 默认是 `Latin1` 编码, 读嵌入的 `qt.conf` 时, 如果 `Prefix = ` 路径带有中文时, 转码就会出错, 导致后面路径加载失败.

## 解决方案

目前是重编 qt, 在 `QConfFileSettingsPrivate` 构造函数的地方加上
```
#if QT_CONFIG(textcodec)
    iniCodec = QTextCodec::codecForLocale();
#endif
```
