
przechodzilem refaktoryzacje z screencap z PNG na RAW , z docka visula buildier nie są prawidlowo wykonywane sekwencej zapisane record action ,
ogólnie prosze o przejzenie projektu oraz zaproponowanie rozszezenie, poprawa wydajnosci /szybkosci. W pliku SwipeBuildierWidget.cpp chyba namieszalem
```
adb_sequence/
├── nlohmann/
│   └── json.hpp
├── argsparser.cpp
├── argsparser.h
├── CMakeLists.txt
├── commandexecutor.cpp
├── commandexecutor.h
├── KeyboardWidget.cpp
├── KeyboardWidget.h
├── main.cpp
├── main_d.cpp
├── mainwindow.cpp
├── mainwindow.h
├── remoteserver.cpp
├── remoteserver.h
├── sequencerunner.cpp
├── sequencerunner.h
├── settingsdialog.cpp
├── settingsdialog.h
├── SwipeBuilderWidget.cpp
├── SwipeBuilderWidget.h
├── SwipeCanvas.cpp
├── SwipeCanvas.h
├── SwipeModel.cpp
├── SwipeModel.h
└── systemcmd.h
```
```
## CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(adb_sequence VERSION 6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebSockets)

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR "Brak pliku nagłówkowego nlohmann/json.hpp.")
endif()
include_directories(nlohmann)

set(SHARED_SOURCES
    argsparser.cpp
    commandexecutor.cpp
    sequencerunner.cpp
)

set(SHARED_HEADERS
    argsparser.h
    commandexecutor.h
    sequencerunner.h
)

add_library(adb_shared_components STATIC
    ${SHARED_SOURCES}
    ${SHARED_HEADERS}
)

target_compile_features(adb_shared_components PRIVATE cxx_std_17)
set_target_properties(adb_shared_components PROPERTIES 
    POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(adb_shared_components PRIVATE
    Qt6::Core
    Qt6::WebSockets
)

qt_add_executable(adb_sequence_d
    main_d.cpp
    remoteserver.cpp
    remoteserver.h
)

target_compile_features(adb_sequence_d PRIVATE cxx_std_17)

target_link_libraries(adb_sequence_d PUBLIC 
    adb_shared_components
    Qt6::Core
    Qt6::Gui
    Qt6::WebSockets
)

set_target_properties(adb_sequence_d PROPERTIES 
    OUTPUT_NAME "adb_sequence_d"
    POSITION_INDEPENDENT_CODE ON 
)

qt_add_executable(adb_sequence
    main.cpp
    mainwindow.cpp
    mainwindow.h
    settingsdialog.cpp
    settingsdialog.h
    SwipeBuilderWidget.cpp
    SwipeBuilderWidget.h
    SwipeCanvas.cpp
    SwipeCanvas.h
    SwipeModel.cpp
    SwipeModel.h
    KeyboardWidget.cpp
    KeyboardWidget.h
)

target_compile_features(adb_sequence PRIVATE cxx_std_17)

target_link_libraries(adb_sequence PUBLIC 
    adb_shared_components
    Qt6::Core 
    Qt6::Widgets
    Qt6::Gui
    Qt6::WebSockets
)

set_target_properties(adb_sequence PROPERTIES 
    OUTPUT_NAME "adb_sequence"
    POSITION_INDEPENDENT_CODE ON
)
```

```
//SwipeBuilderWidget.cpp
#include "SwipeBuilderWidget.h"
#include "SwipeModel.h"
#include "SwipeCanvas.h"
#include "KeyboardWidget.h"
#include "argsparser.h"
#include "commandexecutor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QJsonDocument>
#include <QGroupBox>
#include <QDebug>
#include <QLabel>
#include <QTimer>
#include <QListWidget>
#include <QSplitter>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QRegularExpression>
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QListWidgetItem>
#include <algorithm>

class CommandEditDialog : public QDialog {
    Q_OBJECT
public:
    SwipeAction &m_action;
    CommandEditDialog(SwipeAction &action, QWidget *parent = nullptr)
        : QDialog(parent), m_action(action) {
        setWindowTitle(m_action.type == SwipeAction::Key ? "Edit Key Action" : "Edit Command Action");
        QFormLayout *formLayout = new QFormLayout(this);
        m_commandEdit = new QLineEdit(m_action.command);
        if (m_action.type == SwipeAction::Key) {
            m_commandEdit->setReadOnly(true);
            m_commandEdit->setText(QString("Key Code: %1").arg(m_action.command));
            formLayout->addRow("Key Action:", m_commandEdit);
        } else {
            formLayout->addRow("Command:", m_commandEdit);}
        m_runModeCombo = new QComboBox();
        m_runModeCombo->addItem("adb", "adb");
        m_runModeCombo->addItem("shell", "shell");
        m_runModeCombo->addItem("root", "root");
        int idx = m_runModeCombo->findData(m_action.runMode.toLower());
        if (idx != -1) m_runModeCombo->setCurrentIndex(idx);
        m_delaySpinBox = new QSpinBox();
        m_delaySpinBox->setRange(0, 60000);
        m_delaySpinBox->setSingleStep(100);
        m_delaySpinBox->setValue(m_action.duration); 
        if (m_action.type == SwipeAction::Key) {
            formLayout->addRow("Run Mode:", new QLabel("shell (Fixed for Keyevent)"));
            m_runModeCombo->setEnabled(false);
        } else {
            formLayout->addRow("Run Mode:", m_runModeCombo);}
        formLayout->addRow((m_action.type == SwipeAction::Swipe) ? "Duration (ms):" : "Delay After (ms):", m_delaySpinBox);
        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &CommandEditDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &CommandEditDialog::reject);
        formLayout->addWidget(buttonBox);}
    void accept() override {
        if (m_action.type == SwipeAction::Command) {
            m_action.command = m_commandEdit->text().trimmed();
            m_action.runMode = m_runModeCombo->currentData().toString();
        } else if (m_action.type == SwipeAction::Key) {
        }
        m_action.duration = m_delaySpinBox->value();
        QDialog::accept();
    }
private:
    QLineEdit *m_commandEdit;
    QComboBox *m_runModeCombo;
    QSpinBox *m_delaySpinBox;};

void SwipeBuilderWidget::runFullSequence() {
    if (m_model->actions().isEmpty()) {
        emit adbStatus("Sequence is empty. Add actions first.", true);
        return;}
    emit adbStatus("Sending request to run full sequence...", false);
    emit runFullSequenceRequested();}

void SwipeBuilderWidget::loadJson() {
    QString path = QFileDialog::getOpenFileName(this, "Load Sequence", "", "JSON Files (*.json)");
    if (path.isEmpty()) return;
    loadSequence(path);}

void SwipeBuilderWidget::loadSequence(const QString &filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit adbStatus(QString("Failed to open file: %1").arg(QFileInfo(filePath).fileName()), true);
        return;}
    QByteArray data = f.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isArray()) {
        if (loadSequenceFromJsonArray(doc.array())) {
            emit adbStatus(QString("Sequence loaded from: %1").arg(QFileInfo(filePath).fileName()), false);
        } else {
            emit adbStatus(QString("Failed to parse sequence from: %1").arg(QFileInfo(filePath).fileName()), true);}
    } else {
        emit adbStatus("Invalid JSON format (expected array).", true);}}

SwipeBuilderWidget::SwipeBuilderWidget(CommandExecutor *executor, QWidget *parent)
    : QWidget(parent), m_executor(executor) {
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    m_model = new SwipeModel(this);
    m_canvas = new SwipeCanvas(m_model, this);
    m_canvas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    QSplitter *rightSplitter = new QSplitter(Qt::Vertical);
    QWidget *controlsWidget = new QWidget();
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    m_list = new QListWidget();
    setAcceptDrops(true);    

    controlsLayout->addWidget(new QLabel("Recorded Actions:"));
    controlsLayout->addWidget(m_list);
    QPushButton *b_toggle_keyboard = new QPushButton("Toggle Virtual Keyboard");
    b_toggle_keyboard->setFlat(true);
    b_toggle_keyboard->setStyleSheet("color: #FFAA00;");
    controlsLayout->addWidget(b_toggle_keyboard);
    QHBoxLayout *runLayout = new QHBoxLayout();
    runLayout->setContentsMargins(0, 0, 0, 0);
    m_runButton = new QPushButton("▶ Action");
    m_runButton->setStyleSheet("background-color: #4CAF50; color: white;");
    m_runSequenceButton = new QPushButton("▶ Sequence");
    m_runSequenceButton->setStyleSheet("background-color: #00BCD4; color: white;");
    runLayout->addWidget(m_runButton);
    runLayout->addWidget(m_runSequenceButton);
    controlsLayout->addLayout(runLayout);
    QHBoxLayout *editAddLayout = new QHBoxLayout();
    editAddLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *b_edit = new QPushButton("Edit");
    QPushButton *b_add_cmd = new QPushButton("Add");    
    editAddLayout->addWidget(b_edit);
    editAddLayout->addWidget(b_add_cmd);
    controlsLayout->addLayout(editAddLayout);
    QHBoxLayout *deleteLayout = new QHBoxLayout();
    deleteLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *b_del = new QPushButton("Delete");
    QPushButton *b_clear = new QPushButton("Clear All");
    deleteLayout->addWidget(b_del);
    deleteLayout->addWidget(b_clear);
    controlsLayout->addLayout(deleteLayout);
    QHBoxLayout *fileLayout = new QHBoxLayout();
    fileLayout->setContentsMargins(0, 0, 0, 0);
    QPushButton *b_export = new QPushButton("Save");
    QPushButton *b_import = new QPushButton("Load");
    fileLayout->addWidget(b_export);
    fileLayout->addWidget(b_import);
    controlsLayout->addLayout(fileLayout);
    rightSplitter->addWidget(controlsWidget);
    m_keyboardWidget = new KeyboardWidget(this);
    m_keyboardWidget->hide();
    rightSplitter->addWidget(m_keyboardWidget);
    rightSplitter->setStretchFactor(0, 1);
    rightSplitter->setStretchFactor(1, 0);
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal);
    mainSplitter->addWidget(m_canvas);
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 1);
    mainLayout->addWidget(mainSplitter);
    m_adbProcess = new QProcess(this);
    connect(m_adbProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error){
        m_lastError = error;
        if (error == QProcess::FailedToStart) {
            emit adbStatus("ADB command not found! Check if path is set correctly.", true);
        } else {
            emit adbStatus(QString("ADB process error: %1").arg(error), true);}});
    connect(m_adbProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
             this, &SwipeBuilderWidget::onScreenshotReady);
    m_resolutionProcess = new QProcess(this);
    connect(m_resolutionProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SwipeBuilderWidget::onResolutionReady);
    m_refreshTimer.setInterval(33);    
    connect(&m_refreshTimer, &QTimer::timeout, this, &SwipeBuilderWidget::captureScreenshot);
    connect(m_model, &SwipeModel::modelChanged, this, &SwipeBuilderWidget::updateList);
    connect(b_export, &QPushButton::clicked, this, &SwipeBuilderWidget::saveJson);
    connect(b_import, &QPushButton::clicked, this, &SwipeBuilderWidget::loadJson);
    connect(b_clear, &QPushButton::clicked, this, &SwipeBuilderWidget::clearActions);
    connect(b_del, &QPushButton::clicked, this, &SwipeBuilderWidget::deleteSelected);
    connect(b_edit, &QPushButton::clicked, this, &SwipeBuilderWidget::editSelected);
    connect(b_add_cmd, &QPushButton::clicked, this, &SwipeBuilderWidget::addActionFromDialog);    
    connect(m_runButton, &QPushButton::clicked, this, &SwipeBuilderWidget::runSelectedAction);
    connect(m_runSequenceButton, &QPushButton::clicked, this, &SwipeBuilderWidget::runFullSequence);
    connect(b_toggle_keyboard, &QPushButton::clicked, this, &SwipeBuilderWidget::onKeyboardToggleClicked);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        QMenu menu;
        menu.addAction("Edit", this, &SwipeBuilderWidget::editSelected);
        menu.addAction("Run", this, &SwipeBuilderWidget::runSelectedAction);
        menu.addSeparator();
        menu.addAction("Move Up", this, &SwipeBuilderWidget::moveSelectedUp);
        menu.addAction("Move Down", this, &SwipeBuilderWidget::moveSelectedDown);
        menu.addSeparator();
        menu.addAction("Delete", this, &SwipeBuilderWidget::deleteSelected);
        menu.exec(m_list->mapToGlobal(pos));});
    connect(m_keyboardWidget, &KeyboardWidget::adbCommandGenerated,
             this, &SwipeBuilderWidget::onKeyboardCommandGenerated);
    connect(m_executor, QOverload<int, QProcess::ExitStatus>::of(&CommandExecutor::finished),
             this, &SwipeBuilderWidget::onAdbCommandFinished);
    QTimer::singleShot(500, this, &SwipeBuilderWidget::fetchDeviceResolution);}


SwipeBuilderWidget::~SwipeBuilderWidget() {
    stopMonitoring();
    if (m_adbProcess) {
        m_adbProcess->kill();
        m_adbProcess->waitForFinished(500);}}

void SwipeBuilderWidget::setCanvasStatus(const QString &message, bool isError) {
    m_canvas->setStatus(message, isError);}

void SwipeBuilderWidget::setAdbPath(const QString &path) {
    if (!path.isEmpty()) m_adbPath = path;}

void SwipeBuilderWidget::startMonitoring() {
    if (!m_refreshTimer.isActive() && m_adbProcess->state() == QProcess::NotRunning) {
        m_refreshTimer.start();}}

void SwipeBuilderWidget::stopMonitoring() {
    m_refreshTimer.stop();}

void SwipeBuilderWidget::captureScreenshot() {
    if (m_adbProcess->state() != QProcess::NotRunning) return;
    if (m_deviceWidth == 0 || m_deviceHeight == 0) return; 
    QStringList args;
    QString targetSerial = m_executor->targetDevice();
    if (!targetSerial.isEmpty()) {
        args << "-s" << targetSerial;}
    args << "exec-out" << "screencap"; 
    m_lastError = QProcess::UnknownError;
    m_adbProcess->setProgram(m_adbPath);
    m_adbProcess->setArguments(args);
    m_adbProcess->start();}

void SwipeBuilderWidget::onScreenshotReady(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    if (m_lastError == QProcess::FailedToStart) {
        m_refreshTimer.start();
        return;}
    if (exitCode == 0) {
        QByteArray data = m_adbProcess->readAllStandardOutput();
        int expectedPixelDataSize = m_deviceWidth * m_deviceHeight * 4;
        if (data.size() >= expectedPixelDataSize) {
            int headerSize = data.size() - expectedPixelDataSize;
            if (headerSize >= 0) {
                QByteArray rawPixels = data.mid(headerSize);
                
                m_canvas->loadFromData(rawPixels);
                
                if (m_lastMonitoringStatus != 0) {
                    emit adbStatus("Screenshot OK (RAW).", false);}
                m_lastMonitoringStatus = 0;
            } else {
            }
        } else {
            if (m_lastMonitoringStatus != 1) {
			}}
    } else {
        QByteArray errData = m_adbProcess->readAllStandardError();
        if (m_lastMonitoringStatus != exitCode) {
            emit adbStatus(QString("Screenshot failed. Exit code %1").arg(exitCode), true);
            m_lastMonitoringStatus = exitCode;}}
    m_lastError = QProcess::UnknownError;}

void SwipeBuilderWidget::onKeyboardCommandGenerated(const QString &command) {
    if (command.startsWith("input keyevent")) {
        QString keyName = command.mid(15).trimmed();
        m_model->addKey(keyName, 100); 
    } else {
        m_model->addCommand(command, 100, "shell");}
    QString logCmd = command.simplified();
    if (logCmd.length() > 50) logCmd = logCmd.left(50) + "...";
    emit adbStatus(QString("KEYBOARD: Added: %1").arg(logCmd), false);}

void SwipeBuilderWidget::updateList() {
    m_list->clear();
    int idx = 1;
    for (const auto &a : m_model->actions()) {
        QString text;
        QString durationText = (a.type == SwipeAction::Swipe) ? "T" : "D"; 
        if (a.type == SwipeAction::Tap)
            text = QString("%1. Tap (%2, %3) [%4: %5ms]").arg(idx).arg(a.x1).arg(a.y1).arg(durationText).arg(a.duration);
        else if (a.type == SwipeAction::Swipe)
            text = QString("%1. Swipe (%2, %3) -> (%4, %5) [%4: %6ms]").arg(idx).arg(a.x1).arg(a.y1).arg(a.x2).arg(a.y2).arg(durationText).arg(a.duration);
        else if (a.type == SwipeAction::Command) {
            QString cmdText = a.command.simplified();
            if (cmdText.length() > 40) cmdText = cmdText.left(40) + "...";
            text = QString("%1. CMD [%2]: %3 [%4: %5ms]").arg(idx).arg(a.runMode.toUpper()).arg(cmdText).arg(durationText).arg(a.duration);
        } else if (a.type == SwipeAction::Key) {
            QString cmdText = a.command.simplified();
            text = QString("%1. KEY [%2]: %3 [%4: %5ms]").arg(idx).arg(a.runMode.toUpper()).arg(cmdText).arg(durationText).arg(a.duration);}
        m_list->addItem(text);
        idx++;}
    m_list->scrollToBottom();}

void SwipeBuilderWidget::runSelectedAction() {
    int row = m_list->currentRow();
    if (row < 0 || row >= m_model->actions().count()) {
        emit adbStatus("No action selected.", false);
        return;}
    const SwipeAction &a = m_model->actions().at(row);
    QString fullCommand = getAdbCommandForAction(a, true); 
    QString runMode = "root"; 
    if (a.type == SwipeAction::Command) runMode = a.runMode;
    if (fullCommand.isEmpty()) {
        emit adbStatus("Empty action or command.", true);
        return;}
    QStringList args;
    QStringList shellArgs = ArgsParser::parse(fullCommand); 
    if (runMode.toLower() == "adb") {
        args = shellArgs;
    } else if (runMode.toLower() == "root") {
        args << "shell" << "su" << "-c" << fullCommand;
    } else { 

        if (a.type == SwipeAction::Command) {
             args << "shell" << shellArgs;
        } else {
             args << "shell" << fullCommand;}}
    if (m_executor->isRunning()) {
        emit adbStatus("ADB is busy running another command...", true);
        return;}
    QString logPrefix = runMode.toUpper();
    QString logCmd = args.join(' ').simplified();
    if (logCmd.length() > 80) logCmd = logCmd.left(80) + "...";
    emit adbStatus(QString("EXEC (%1): %2").arg(logPrefix).arg(logCmd), false);
    m_executor->runAdbCommand(args);
    QListWidgetItem *item = m_list->item(row);
    if (item) item->setBackground(QBrush(QColor("#FFC107")));}


void SwipeBuilderWidget::onAdbCommandFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    int finishedRow = -1;
    
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (item->background().color() == QColor("#FFC107")) {
            finishedRow = i;
            item->setBackground(Qt::NoBrush); 
            break;}}
    if (exitCode == 0) {
        if (finishedRow != -1) {
            QListWidgetItem *item = m_list->item(finishedRow);
            if (item) item->setBackground(QBrush(QColor("#4CAF50"))); 
            
            QTimer::singleShot(200, this, [item](){ if (item) item->setBackground(Qt::NoBrush); });}
        emit adbStatus("OK", false);
    } else {
        if (finishedRow != -1) {
            QListWidgetItem *item = m_list->item(finishedRow);
            if (item) item->setBackground(QBrush(QColor("#F44336"))); 
            QTimer::singleShot(500, this, [item](){ if (item) item->setBackground(Qt::NoBrush); });}
        emit adbStatus(QString("Error: %1").arg(exitCode), true);}}

void SwipeBuilderWidget::saveJson() {
    QString path = QFileDialog::getSaveFileName(this, "Save Sequence", "", "JSON Files (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_model->toJsonSequence());
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();
        emit sequenceGenerated(path);
    } else {
        emit adbStatus("Failed to save file.", true);}}


bool SwipeBuilderWidget::loadSequenceFromJsonArray(const QJsonArray &array) {
    m_model->clear();
    bool success = true;
    QRegularExpression tapRegex("^input\\s+tap\\s+(\\d+)\\s+(\\d+)(?:\\s+\\d+)?$");
    QRegularExpression swipeRegex("^input\\s+swipe\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)(?:\\s+(\\d+))?$");
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            success = false;
            break;}
        QJsonObject obj = value.toObject();
        QString command = obj.value("command").toString().trimmed();
        int delayMs = obj.value("delayAfterMs").toInt(100);
        QString runMode = obj.value("runMode").toString("shell").toLower();
        QRegularExpressionMatch tapMatch = tapRegex.match(command);
        QRegularExpressionMatch swipeMatch = swipeRegex.match(command);
        if (tapMatch.hasMatch()) {
            int x = tapMatch.captured(1).toInt();
            int y = tapMatch.captured(2).toInt();
            m_model->addTap(x, y); 
            if (m_model->actions().last().type == SwipeAction::Tap) {
                SwipeAction a = m_model->actions().last();
                a.duration = delayMs; 
                a.runMode = runMode;
                m_model->editActionAt(m_model->actions().count() - 1, a);}
        } else if (swipeMatch.hasMatch()) {
            int duration = swipeMatch.captured(5).toInt(); 
            m_model->addSwipe(swipeMatch.captured(1).toInt(),
                                    swipeMatch.captured(2).toInt(),
                                    swipeMatch.captured(3).toInt(),
                                    swipeMatch.captured(4).toInt(),
                                    duration); 
            if (m_model->actions().last().type == SwipeAction::Swipe) {
                SwipeAction a = m_model->actions().last();
                a.duration = delayMs; 
                a.runMode = runMode;
                m_model->editActionAt(m_model->actions().count() - 1, a);}
        } else if (command.startsWith("input keyevent")) {
            QString keyName = command.mid(15).trimmed();
            if (!keyName.isEmpty()) {
                m_model->addKey(keyName, delayMs);
            } else {
                m_model->addCommand(command, delayMs, runMode);}
        } else {
            m_model->addCommand(command, delayMs, runMode);}}
    return success;}

QString SwipeBuilderWidget::getAdbCommandForAction(const SwipeAction &action, bool forceRoot) const {
    QString command;
    Q_UNUSED(forceRoot); 
    
    switch (action.type) {
        case SwipeAction::Tap:
            command = QString("input tap %1 %2").arg(action.x1).arg(action.y1);
            break;
        case SwipeAction::Swipe:
            command = QString("input swipe %1 %2 %3 %4 %5") 
                      .arg(action.x1).arg(action.y1)
                      .arg(action.x2).arg(action.y2)
                      .arg(action.duration); 
            break;
        case SwipeAction::Command:
            command = action.command;
            break;
        case SwipeAction::Key:
            command = QString("input keyevent %1").arg(action.command);
            break;}
    return command;}

void SwipeBuilderWidget::clearActions() { m_model->clear(); }

void SwipeBuilderWidget::deleteSelected() {
    int row = m_list->currentRow();
    if (row >= 0) m_model->removeActionAt(row);}

void SwipeBuilderWidget::onKeyboardToggleClicked() {
    m_keyboardWidget->setVisible(!m_keyboardWidget->isVisible());
    QPushButton *senderBtn = qobject_cast<QPushButton*>(sender());
    if (senderBtn) {
        senderBtn->setText(m_keyboardWidget->isVisible() ? "Hide Virtual Keyboard" : "Toggle Virtual Keyboard");}}

void SwipeBuilderWidget::editSelected() {
    int row = m_list->currentRow();
    if (row < 0) return;
    SwipeAction currentAction = m_model->actionAt(row);
    if (currentAction.type == SwipeAction::Command || currentAction.type == SwipeAction::Key) {
        CommandEditDialog dialog(currentAction, this);
        if (dialog.exec() == QDialog::Accepted) {
            m_model->editActionAt(row, currentAction);}
    } else {
        setCanvasStatus("Use Delete & Re-add for coordinates.", false); }}

void SwipeBuilderWidget::moveSelectedUp() {
    int row = m_list->currentRow();
    if (row > 0) {
        m_model->moveAction(row, row - 1);
        m_list->setCurrentRow(row - 1);}}

void SwipeBuilderWidget::moveSelectedDown() {
    int row = m_list->currentRow();
    if (row >= 0 && row < m_model->actions().count() - 1) {
        m_model->moveAction(row, row + 1);
        m_list->setCurrentRow(row + 1);}}

void SwipeBuilderWidget::addActionFromDialog() {
    SwipeAction tempAction(SwipeAction::Command, 0, 0, 0, 0, 100, "", "shell");
    CommandEditDialog dialog(tempAction, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_model->addCommand(tempAction.command, tempAction.duration, tempAction.runMode);
        emit adbStatus(QString("Added Command: %1").arg(tempAction.command.simplified()), false);}}

void SwipeBuilderWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        bool hasJsonFile = false;
        for (const QUrl &url : event->mimeData()->urls()) {
            if (url.isLocalFile() && QFileInfo(url.toLocalFile()).suffix().compare("json", Qt::CaseInsensitive) == 0) {
                hasJsonFile = true;
                break;}}
        if (hasJsonFile) {
            event->acceptProposedAction();
            return;}}
    event->ignore();}

void SwipeBuilderWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();}}

void SwipeBuilderWidget::dropEvent(QDropEvent *event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).suffix().compare("json", Qt::CaseInsensitive) == 0) {
            loadSequence(url.toLocalFile());
            break; }}
    event->acceptProposedAction();}

void SwipeBuilderWidget::fetchDeviceResolution() {
    if (m_resolutionProcess->state() != QProcess::NotRunning) return;
    emit adbStatus("Fetching device resolution (adb shell wm size)...", false);
    QStringList args;
    QString targetSerial = m_executor->targetDevice();
    if (!targetSerial.isEmpty()) {
        args << "-s" << targetSerial;}
    args << "shell" << "wm" << "size";
    m_resolutionProcess->setProgram(m_adbPath);
    m_resolutionProcess->setArguments(args);
    m_resolutionProcess->start();}

void SwipeBuilderWidget::onResolutionReady(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    if (exitCode == 0) {
        QByteArray output = m_resolutionProcess->readAllStandardOutput();
        // Regex szukający np. "Physical size: 1080x1920"
        QRegularExpression rx("Physical size:\\s*(\\d+)x(\\d+)");
        QRegularExpressionMatch match = rx.match(QString::fromUtf8(output));
        if (match.hasMatch()) {
            m_deviceWidth = match.captured(1).toInt();
            m_deviceHeight = match.captured(2).toInt();
            m_canvas->setDeviceResolution(m_deviceWidth, m_deviceHeight);
            emit adbStatus(QString("Resolution found: %1x%2. Starting RAW capture.").arg(m_deviceWidth).arg(m_deviceHeight), false);
            startMonitoring(); 
        } else {
            emit adbStatus("Failed to parse device resolution from 'wm size'. Is device connected?", true);
            QTimer::singleShot(2000, this, &SwipeBuilderWidget::fetchDeviceResolution);}
    } else {
        emit adbStatus(QString("Error fetching resolution. Exit code %1").arg(exitCode), true);}}

void SwipeBuilderWidget::setRunSequenceButtonEnabled(bool enabled) { 
    if (m_runSequenceButton) {
        m_runSequenceButton->setEnabled(enabled);}}

#include "SwipeBuilderWidget.moc"
```
```
//SwipeBuilderWidget.h
#pragma once
#include <QWidget>
#include <QProcess>
#include <QTimer>
#include <QListWidget>
#include "SwipeModel.h"
#include "SwipeCanvas.h"
#include "KeyboardWidget.h"
#include "commandexecutor.h"
#include <QJsonArray>
#include <QPushButton>

class SwipeBuilderWidget : public QWidget {
    Q_OBJECT
public:
    explicit SwipeBuilderWidget(CommandExecutor *executor, QWidget *parent = nullptr);
    ~SwipeBuilderWidget();

    void setAdbPath(const QString &path);
    void startMonitoring();
    void stopMonitoring();
    void captureScreenshot();
    void setCanvasStatus(const QString &message, bool isError);
    SwipeModel *model() const { return m_model; }
    void loadSequence(const QString &filePath);
    void setRunSequenceButtonEnabled(bool enabled);

signals:
    void adbStatus(const QString &message, bool isError);
    void sequenceGenerated(const QString &filePath);
    void runFullSequenceRequested();

private slots:
    void onScreenshotReady(int exitCode, QProcess::ExitStatus exitStatus);
    void onKeyboardCommandGenerated(const QString &command);
    void onKeyboardToggleClicked();
    void updateList();
    void saveJson();
    void loadJson(); 
    void clearActions();
    void deleteSelected();
    void runSelectedAction();
    void runFullSequence();
    void editSelected();
    void moveSelectedUp();
    void moveSelectedDown();
    void onAdbCommandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void addActionFromDialog(); 
    void onResolutionReady(int exitCode, QProcess::ExitStatus exitStatus);
    void fetchDeviceResolution();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    
private:
    SwipeModel *m_model = nullptr;
    SwipeCanvas *m_canvas = nullptr;
    KeyboardWidget *m_keyboardWidget = nullptr;
    QListWidget *m_list = nullptr;
    int m_lastMonitoringStatus = -1;
    int m_deviceWidth = 0;
    int m_deviceHeight = 0;
    QProcess *m_adbProcess = nullptr;
    QProcess *m_resolutionProcess = nullptr;
    QString m_adbPath = QStringLiteral("adb");
    QTimer m_refreshTimer;
    QProcess::ProcessError m_lastError;
    CommandExecutor *m_executor = nullptr;
    QPushButton *m_runButton = nullptr; 
    QPushButton *m_runSequenceButton = nullptr;
    bool loadSequenceFromJsonArray(const QJsonArray &array);
    QString getAdbCommandForAction(const SwipeAction &action, bool forceRoot = false) const;
};
```

```
//SwipeModel.cpp
#include "SwipeModel.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>
#include <QVector>

SwipeModel::SwipeModel(QObject *parent) : QObject(parent) {}

void SwipeModel::addTap(int x, int y) {
    m_actions.append(SwipeAction(SwipeAction::Tap, x, y, 0, 0, 0, QString(), "root"));
    emit modelChanged();}

void SwipeModel::addSwipe(int x1, int y1, int x2, int y2, int duration) {
    m_actions.append(SwipeAction(SwipeAction::Swipe, x1, y1, x2, y2, duration, QString(), "root"));
    emit modelChanged();}

void SwipeModel::addCommand(const QString &command, int delayMs, const QString &runMode) {
    m_actions.append(SwipeAction(SwipeAction::Command, 0, 0, 0, 0, delayMs, command, runMode));
    emit modelChanged();}

void SwipeModel::addKey(const QString &keyName, int delayMs) {
    m_actions.append(SwipeAction(SwipeAction::Key, 0, 0, 0, 0, delayMs, keyName, "root"));
    emit modelChanged();}

SwipeAction SwipeModel::actionAt(int index) const {
    if (index >= 0 && index < m_actions.size()) {
        return m_actions.at(index);}
    return SwipeAction(SwipeAction::Command, 0, 0, 0, 0, 0, QString(), "shell");}

void SwipeModel::editActionAt(int index, const SwipeAction &action) {
    if (index >= 0 && index < m_actions.size()) {
        m_actions[index] = action;
        emit modelChanged();}}

void SwipeModel::moveAction(int from, int to) {
    if (from != to && from >= 0 && from < m_actions.size() && to >= 0 && to < m_actions.size()) {
        m_actions.move(from, to);
        emit modelChanged();}}

void SwipeModel::clear() {
    m_actions.clear();
    emit modelChanged();}

void SwipeModel::removeActionAt(int index) {
    if (index >= 0 && index < m_actions.size()) {
        m_actions.removeAt(index);
        emit modelChanged();}}

QJsonArray SwipeModel::toJsonSequence() const {
    QJsonArray array;
    for (const SwipeAction &action : m_actions) {
        array.append(actionToJson(action));}
    return array;}

QJsonObject SwipeModel::actionToJson(const SwipeAction &action) const {
    QJsonObject obj;
    QString finalCommand;
    if (action.type == SwipeAction::Swipe) {
        obj["delayAfterMs"] = 100;
    } else {
        obj["delayAfterMs"] = action.duration; }
    obj["runMode"] = action.runMode.isEmpty() ? "shell" : action.runMode.toLower(); 
    obj["stopOnError"] = true;

    switch (action.type) {
        case SwipeAction::Tap:
            finalCommand = QString("input tap %1 %2").arg(action.x1).arg(action.y1);
            obj["runMode"] = "shell";
            break;
        case SwipeAction::Swipe:
            finalCommand = QString("input swipe %1 %2 %3 %4 %5")
                           .arg(action.x1).arg(action.y1)
                           .arg(action.x2).arg(action.y2)
                           .arg(action.duration);
            obj["runMode"] = "shell";
            break;
        case SwipeAction::Key:
            finalCommand = QString("input keyevent %1").arg(action.command);
            obj["runMode"] = "shell";
            break;
        case SwipeAction::Command:
            finalCommand = action.command;
            break;}
    obj["command"] = finalCommand;
    return obj;}
```

```
//SwipeModel.h
#pragma once
#include <QObject>
#include <QPoint>
#include <QVector>
#include <QJsonArray>
#include <QJsonObject>

struct SwipeAction {
    enum Type {
        Tap,
        Swipe,
        Command,
        Key
    };
    
    Type type;
    int x1, y1;
    int x2, y2;
    int duration;
    QString command;
    QString runMode;

    SwipeAction(Type t, int x1, int y1, int x2 = 0, int y2 = 0, int duration = 0, 
                const QString &cmd = QString(), const QString &mode = "shell")
        : type(t), x1(x1), y1(y1), x2(x2), y2(y2), duration(duration), command(cmd), runMode(mode) {}
};

class SwipeModel : public QObject {
    Q_OBJECT
public:
    explicit SwipeModel(QObject *parent = nullptr);
    void addTap(int x, int y);
    void addSwipe(int x1, int y1, int x2, int y2, int duration);
    void addCommand(const QString &command, int delayMs = 100, const QString &runMode = "shell"); 
    void addKey(const QString &keyCommand, int delayMs = 100);

    void editActionAt(int index, const SwipeAction &action);
    void moveAction(int from, int to);
    SwipeAction actionAt(int index) const;
    void clear();
    void removeActionAt(int index);
    QVector<SwipeAction> actions() const { return m_actions; }
    int actionCount() const { return m_actions.count(); }
    
    QJsonArray toJsonSequence() const;
    QJsonObject actionToJson(const SwipeAction &action) const; // NOWA METODA
    
signals:
    void modelChanged();

private:
    QVector<SwipeAction> m_actions;
};
```

```
// SwipeCanvas.cpp
#include "SwipeCanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>

SwipeCanvas::SwipeCanvas(SwipeModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);}

void SwipeCanvas::setStatus(const QString &msg, bool isError) {
    m_statusMessage = msg;
    m_isErrorStatus = isError;
    update();}

void SwipeCanvas::setDeviceResolution(int width, int height) {
    m_deviceWidth = width;
    m_deviceHeight = height;
    if (width > 0 && height > 0 && m_img.isNull()) {
        setStatus("Device resolution known. Ready to capture in RAW mode.", false);}}

void SwipeCanvas::loadFromData(const QByteArray &data) {
    if (m_deviceWidth == 0 || m_deviceHeight == 0) {
        if (m_img.loadFromData(data)) {
            m_statusMessage.clear(); 
            update();
            return;}
        setStatus("Resolution unknown. Cannot process RAW data or load PNG.", true);
        return;}
    int bytesPerPixel = 4;
    int expectedSize = m_deviceWidth * m_deviceHeight * bytesPerPixel;
    if (data.size() < expectedSize) {
        setStatus(QString("RAW error: Expected %1 bytes (Resolution: %2x%3), got %4. Check if the device is disconnected or the screen format is different (e.g., RGB565).")
                  .arg(expectedSize).arg(m_deviceWidth).arg(m_deviceHeight).arg(data.size()), true);
        return;}
    QImage rawImage((const uchar*)data.constData(),
                    m_deviceWidth,
                    m_deviceHeight,
                    QImage::Format_RGBA8888); 

    if (!rawImage.isNull()) {
        m_img = rawImage.convertToFormat(QImage::Format_ARGB32); 
        m_statusMessage.clear();
        update();
    } else {
        setStatus("Failed to create QImage from RAW data (Check internal format constant).", true);}}

void SwipeCanvas::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);
    if (m_img.isNull() || !m_statusMessage.isEmpty()) {
        p.setPen(m_isErrorStatus ? Qt::red : Qt::white);
        QString msg = m_statusMessage.isEmpty()
                      ? "Waiting for screen... (Check ADB connection)" 
                      : m_statusMessage;
        p.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, msg);
        return;}
    double wRatio = (double)width() / m_img.width();
    double hRatio = (double)height() / m_img.height();
    m_scaleFactor = std::min(wRatio, hRatio);
    int drawnW = m_img.width() * m_scaleFactor;
    int drawnH = m_img.height() * m_scaleFactor;
    m_offsetX = (width() - drawnW) / 2;
    m_offsetY = (height() - drawnH) / 2;
    QRect targetRect(m_offsetX, m_offsetY, drawnW, drawnH);
    p.drawImage(targetRect, m_img);
    if (m_dragging) {
        p.setPen(QPen(Qt::green, 3)); 
        p.drawLine(m_start, m_end);
        p.setBrush(Qt::green);
        p.drawEllipse(m_start, 5, 5);
        p.setBrush(Qt::red);
        p.drawEllipse(m_end, 5, 5);}}

QPoint SwipeCanvas::mapToDevice(QPoint p) {
    if (m_img.isNull() || m_scaleFactor == 0) return QPoint(0,0);
    int devX = (p.x() - m_offsetX) / m_scaleFactor;
    int devY = (p.y() - m_offsetY) / m_scaleFactor;
    devX = std::max(0, std::min(devX, m_img.width()));
    devY = std::max(0, std::min(devY, m_img.height()));
    return QPoint(devX, devY);}

void SwipeCanvas::mousePressEvent(QMouseEvent *e) {
    if (m_img.isNull()) return;
    m_start = e->pos();
    m_end = m_start;
    m_dragging = true;
    update();}

void SwipeCanvas::mouseMoveEvent(QMouseEvent *e) {
    if (m_dragging) {
        m_end = e->pos();
        update();}}

void SwipeCanvas::mouseReleaseEvent(QMouseEvent *e) {
    if (!m_dragging) return;
    m_end = e->pos();
    QPoint devStart = mapToDevice(m_start);
    QPoint devEnd = mapToDevice(m_end);
    if ((m_start - m_end).manhattanLength() < 5) {
        m_model->addTap(devStart.x(), devStart.y());
    } else {
        m_model->addSwipe(devStart.x(), devStart.y(), devEnd.x(), devEnd.y(), 500);}
    m_dragging = false;
    update();}
```

```
// SwipeCanvas.h
#pragma once
#include <QWidget>
#include <QImage>
#include <QPoint>
#include "SwipeModel.h"

class SwipeCanvas : public QWidget {
    Q_OBJECT
public:
    explicit SwipeCanvas(SwipeModel *model, QWidget *parent = nullptr);
    void loadFromData(const QByteArray &data);
    
    void setStatus(const QString &msg, bool isError);
    void setDeviceResolution(int width, int height);

signals:
    void tapAdded(int x, int y);
    void swipeAdded(int x1, int y1, int x2, int y2, int duration);
    
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    SwipeModel *m_model;
    QImage m_img;
    bool m_dragging = false;
    QPoint m_start;
    QPoint m_end;
    QPoint mapToDevice(QPoint p);
    double m_scaleFactor = 1.0;
    int m_offsetX = 0;
    int m_offsetY = 0;
    QString m_statusMessage; 
    bool m_isErrorStatus = false;
    int m_deviceWidth = 0;
    int m_deviceHeight = 0;
};
```

```
//commandexecutor.cpp
#include "commandexecutor.h"
#include <QByteArray>
#include <QDebug>
#include <QProcess>
#include <QCoreApplication>

CommandExecutor::CommandExecutor(QObject *parent) : QObject(parent) {
    m_adbPath = "adb";
    m_targetSerial = QString();
    m_shellProcess = new QProcess(this);
    connect(m_shellProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &CommandExecutor::onShellProcessFinished);
    connect(m_shellProcess, &QProcess::readyReadStandardOutput, this, &CommandExecutor::readStdOut);
    connect(m_shellProcess, &QProcess::readyReadStandardError, this, &CommandExecutor::readStdErr);}

CommandExecutor::~CommandExecutor() {
    stop();}

void CommandExecutor::setAdbPath(const QString &path) {
    if (!path.isEmpty()) m_adbPath = path;
}

void CommandExecutor::setTargetDevice(const QString &serial) {
    m_targetSerial = serial;
    // Restart stałego shella jest konieczny, aby używał nowego numeru seryjnego
    if (m_shellProcess && m_shellProcess->state() == QProcess::Running) {
        m_shellProcess->kill();
        m_shellProcess->waitForFinished(500); // Czekaj na zakończenie
    }
}

void CommandExecutor::stop() {
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();}
    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;}
    if (m_shellProcess && m_shellProcess->state() == QProcess::Running) {
        m_shellProcess->kill(); // Wymuś zamknięcie stałego połączenia
        m_shellProcess->waitForFinished(500);}}

void CommandExecutor::cancelCurrentCommand() {
    stop();}

bool CommandExecutor::isRunning() const {
    return m_process && m_process->state() == QProcess::Running; }

void CommandExecutor::runAdbCommand(const QStringList &args) {
    stop();
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &CommandExecutor::readStdOut);
    connect(m_process, &QProcess::readyReadStandardError, this, &CommandExecutor::readStdErr);
    connect(m_process, &QProcess::started, this, &CommandExecutor::onStarted);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
    this, &CommandExecutor::onFinished);
    QStringList finalArgs;
    if (!m_targetSerial.isEmpty()) {
        finalArgs << "-s" << m_targetSerial;}
    finalArgs.append(args);
    m_process->start(m_adbPath, finalArgs);}

void CommandExecutor::executeAdbCommand(const QString &command) {
    QStringList args = command.split(' ', Qt::SkipEmptyParts);
    runAdbCommand(args);}

void CommandExecutor::executeShellCommand(const QString &command) {
    ensureShellRunning();
    
    if (m_shellProcess->state() != QProcess::Running) {
        qCritical() << "Cannot execute shell command, persistent shell is not running.";
        return;}
    QByteArray cmdData = (command + "\n").toUtf8();
    m_shellProcess->write(cmdData);}

void CommandExecutor::executeRootShellCommand(const QString &command) {
    runAdbCommand(QStringList() << "shell" << "su" << "-c" << command);}

void CommandExecutor::ensureShellRunning() {
    if (m_shellProcess && m_shellProcess->state() == QProcess::Running) {
        return;}
    qDebug() << "Starting persistent ADB shell process...";
    QStringList args;
    if (!m_targetSerial.isEmpty()) {
        args << "-s" << m_targetSerial;}
    args << "shell";
    m_shellProcess->start(m_adbPath, args);
    if (!m_shellProcess->waitForStarted(5000)) { 
        qCritical() << "Failed to start persistent ADB shell!";
    } else {
        qDebug() << "Persistent ADB shell started.";}}

void CommandExecutor::onShellProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qWarning() << "Persistent Shell process finished unexpectedly. Exit code:" << exitCode << "Status:" << exitStatus;}

void CommandExecutor::readStdOut() {
    if (sender() == m_process) {
        const QByteArray data = m_process->readAllStandardOutput();
        if (!data.isEmpty()) emit outputReceived(QString::fromUtf8(data));
    } else if (sender() == m_shellProcess) {
        const QByteArray data = m_shellProcess->readAllStandardOutput();
        if (!data.isEmpty()) {
            emit outputReceived("[SHELL] " + QString::fromUtf8(data));}}}

void CommandExecutor::readStdErr() {
    if (sender() == m_process) {
        const QByteArray data = m_process->readAllStandardError();
        if (!data.isEmpty()) emit errorReceived(QString::fromUtf8(data));
    } else if (sender() == m_shellProcess) {
        const QByteArray data = m_shellProcess->readAllStandardError();
        if (!data.isEmpty()) {
             emit errorReceived("[SHELL ERROR] " + QString::fromUtf8(data));}}}

void CommandExecutor::onStarted() {
    emit started();}

void CommandExecutor::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    emit finished(exitCode, exitStatus);}
```

```
//commandexecutor.h
#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>

class CommandExecutor : public QObject {
    Q_OBJECT
public:
    explicit CommandExecutor(QObject *parent = nullptr);
    ~CommandExecutor();

    void setAdbPath(const QString &path);
    void setTargetDevice(const QString &serial);
    void runAdbCommand(const QStringList &args);
    void executeShellCommand(const QString &command);
    void executeRootShellCommand(const QString &command);
    void executeAdbCommand(const QString &command);
    void stop();
    void cancelCurrentCommand(); 
    QString adbPath() const { return m_adbPath; }
    QString targetDevice() const { return m_targetSerial; }
    bool isRunning() const;

signals:
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void outputReceived(const QString &output);
    void errorReceived(const QString &error);

private slots:
    void readStdOut();
    void readStdErr();
    void onStarted();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onShellProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);


private:
    void ensureShellRunning();
    QString m_adbPath;
    QString m_targetSerial;
    QProcess *m_process = nullptr;
    QProcess *m_shellProcess = nullptr; 
};
```

```
//mainwindow.cpp
#include "mainwindow.h"
#include "SwipeBuilderWidget.h"
#include "commandexecutor.h"
#include "settingsdialog.h"
#include "sequencerunner.h"
#include "argsparser.h"
#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <QApplication>
#include <QCoreApplication>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QDockWidget>
#include <QListWidget>
#include <QMenuBar>
#include <QInputDialog>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLineEdit>
#include <QTextEdit>
#include <QMessageBox>
#include <QFileInfo>
#include <QEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QDir>
#include <QHeaderView>
#include <QAction>
#include <QCloseEvent>
#include <QMenu>
#include <QContextMenuEvent>
#include <QStandardItem>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QComboBox>
#include <QRegularExpression>

class LogDialog : public QDialog {
public:
    explicit LogDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("headless");
        resize(900, 400);
        setStyleSheet("background: #000; border: none;");
        auto layout = new QVBoxLayout(this);
        m_output = new QTextEdit();
        m_output->setReadOnly(true);
        m_output->setStyleSheet("background: #000; color: #f0f0f0; border: none; font-family: monospace;");
        layout->addWidget(m_output);
        m_fontSize = m_output->font().pointSize();
        installEventFilter(this);}
    void setDocument(QTextDocument *doc) {if (doc) m_output->setDocument(doc);}
    void appendText(const QString &text, const QColor &color = QColor("#f0f0f0")) {
        m_output->setTextColor(color);
        m_output->append(text);
        m_output->setTextColor(QColor("#f0f0f0"));}
    void clear() { m_output->clear(); }
    QString text() const { return m_output->toPlainText(); }
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
    if (event->type() == QEvent::KeyPress) {QKeyEvent *ke = static_cast<QKeyEvent*>(event);
    if (ke->modifiers() == Qt::ControlModifier && ke->key() == Qt::Key_C) {return false;}}
    return QDialog::eventFilter(obj, event);}
    void wheelEvent(QWheelEvent *event) override {
    if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
    int delta = event->angleDelta().y();
    if (delta > 0) m_fontSize = qMin(m_fontSize + 1, 32);
    else m_fontSize = qMax(m_fontSize - 1, 8);
    QFont f = m_output->font(); f.setPointSize(m_fontSize); m_output->setFont(f);
    event->accept();} else QDialog::wheelEvent(event);}
private:
    QTextEdit *m_output = nullptr;
    int m_fontSize = 9;};

MainWindow::MainWindow(QWidget *parent, const QString &adbPath, const QString &targetSerial)
    : QMainWindow(parent)
{
    setWindowTitle("Android Debug Bridge Sequence");
    resize(1100, 720);
    setAcceptDrops(true);
    ensureJsonPathLocal();
    m_executor = new CommandExecutor(this);
    m_executor->setAdbPath(adbPath);
    m_executor->setTargetDevice(targetSerial);
    m_commandTimer = new QTimer(this);
    connect(m_commandTimer, &QTimer::timeout, this, &MainWindow::executeScheduledCommand);
    m_sequenceRunner = new SequenceRunner(m_executor, this);
    connect(m_sequenceRunner, &SequenceRunner::sequenceStarted, this, &MainWindow::onSequenceStarted);
    connect(m_sequenceRunner, &SequenceRunner::sequenceFinished, this, &MainWindow::onSequenceFinished);
    connect(m_sequenceRunner, &SequenceRunner::commandExecuting, this, &MainWindow::onSequenceCommandExecuting);
    connect(m_sequenceRunner, &SequenceRunner::logMessage, this, &MainWindow::handleSequenceLog);
    m_sequenceIntervalTimer = new QTimer(this);
    m_sequenceIntervalTimer->setSingleShot(true);
    connect(m_sequenceIntervalTimer, &QTimer::timeout, this, &MainWindow::startIntervalSequence);
    connect(m_sequenceRunner, &SequenceRunner::scheduleRestart, this, [this](int interval){
        if (m_sequenceIntervalTimer->isActive()) m_sequenceIntervalTimer->stop();
        m_sequenceIntervalTimer->setInterval(interval * 1000);
        m_sequenceIntervalTimer->start();
        updateTimerDisplay();
    });
    m_displayTimer = new QTimer(this);
    m_displayTimer->setInterval(100);
    connect(m_displayTimer, &QTimer::timeout, this, &MainWindow::updateTimerDisplay);
    m_displayTimer->start();
    m_isRootShell = m_settings.value("isRootShell", false).toBool();
    connect(m_executor, &CommandExecutor::outputReceived, this, &MainWindow::onOutput);
    connect(m_executor, &CommandExecutor::errorReceived, this, &MainWindow::onError);
    connect(m_executor, &CommandExecutor::started, this, &MainWindow::onProcessStarted);
    connect(m_executor, &CommandExecutor::finished, this, &MainWindow::onProcessFinished);
    setupMenus();
    // Doki: Kategorie
    m_categoryList = new QListWidget();
    m_categoryList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_categoryList, &QListWidget::currentItemChanged, this, &MainWindow::onCategoryChanged);
    m_dockCategories = new QDockWidget(tr("Categories"), this);
    m_dockCategories->setObjectName("dockCategories");
    m_dockCategories->setWidget(m_categoryList);
    m_dockCategories->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCategories);
    // Doki: Komendy
    m_commandModel = new QStandardItemModel(this);
    m_commandModel->setHorizontalHeaderLabels(QStringList{"Command", "Description"});
    m_commandProxy = new QSortFilterProxyModel(this);
    m_commandProxy->setSourceModel(m_commandModel);
    m_commandProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_commandProxy->setFilterKeyColumn(-1);
    m_commandView = new QTreeView();
    m_commandView->setModel(m_commandProxy);
    m_commandView->setRootIsDecorated(false);
    m_commandView->setAlternatingRowColors(true);
    m_commandView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_commandView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_commandView->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_commandView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_commandView, &QTreeView::doubleClicked, this, &MainWindow::onCommandDoubleClicked);
    // Kontekstowe menu
    m_commandView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_commandView, &QWidget::customContextMenuRequested, this, [this](const QPoint &pt){
        QModelIndex idx = m_commandView->indexAt(pt);
        if (!idx.isValid()) return;
        QModelIndex s = m_commandProxy->mapToSource(idx);
        if (!s.isValid()) return;
        QString cmd = m_commandModel->item(s.row(), 0)->text();
        QMenu menu(this);
        menu.addAction("Execute", [this, cmd](){ m_commandEdit->setText(cmd); runCommand(); });
        menu.addAction("Edit", [this, s](){ m_commandView->selectionModel()->clear(); m_commandView->setCurrentIndex(m_commandProxy->mapFromSource(s)); editCommand(); });
        menu.addAction("Remove", [this, s](){ m_commandView->selectionModel()->clear(); m_commandView->setCurrentIndex(m_commandProxy->mapFromSource(s)); removeCommand(); });
        menu.exec(m_commandView->viewport()->mapToGlobal(pt));
    });
    m_dockCommands = new QDockWidget(tr("Commands"), this);
    m_dockCommands->setObjectName("dockCommands");
    m_dockCommands->setWidget(m_commandView);
    m_dockCommands->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::TopDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCommands);
    splitDockWidget(m_dockCategories, m_dockCommands, Qt::Horizontal);
    // Doki: Logi
    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setStyleSheet("background: #000; color: #f0f0f0; font-family: monospace;");
    m_dockLog = new QDockWidget(tr("Execution Console"), this);
    m_dockLog->setObjectName("dockLog");
    m_dockLog->setWidget(m_log);
    m_dockLog->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_dockLog);
    m_dockControls = new QDockWidget(tr("Controls"), this);
    m_dockControls->setObjectName("dockControls");
    m_dockControls->setWidget(createControlsWidget());
    m_dockControls->setAllowedAreas(Qt::TopDockWidgetArea | Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::TopDockWidgetArea, m_dockControls);
    connect(m_commandView, &QTreeView::clicked, [this](const QModelIndex &idx){
        QModelIndex s = m_commandProxy->mapToSource(idx);
        if (s.isValid()) {
            QString cmd = m_commandModel->item(s.row(), 0)->text();
            
            if (m_commandEdit) {
                m_commandEdit->setText(cmd);}
            if (m_dockControls) {
                m_dockControls->setVisible(true);
                m_dockControls->raise();
                if (m_commandEdit) { m_commandEdit->setFocus(); m_commandEdit->selectAll(); } }}});

    setupSequenceDock();
    // Doki: Visual JSON/SwipeBuilder
    m_swipeBuilder = new SwipeBuilderWidget(m_executor, this);
    m_swipeBuilder->setAdbPath(m_executor->adbPath());
    connect(m_swipeBuilder, &SwipeBuilderWidget::adbStatus,
            this, [this](const QString &message, bool isError) {
        appendLog(message, isError ? "#F44336" : "#00BCD4");});
    connect(m_swipeBuilder, &SwipeBuilderWidget::runFullSequenceRequested, this, [this](){
        if (m_swipeBuilder->model()->actions().isEmpty()) {
            appendLog("Visual JSON Builder: Cannot run empty sequence.", "#F44336");
            return;
        }
        QJsonArray sequenceArray = m_swipeBuilder->model()->toJsonSequence();
        m_sequenceRunner->clearSequence();
        if (m_sequenceRunner->loadSequenceFromJsonArray(sequenceArray)) {
            appendLog("Visual JSON Builder: Loaded sequence into Runner. Starting execution...", "#4CAF50");
            m_sequenceRunner->startSequence();
            if (m_dockSequence) {
                 m_dockSequence->show();
                 m_dockSequence->raise();}
        } else {
            appendLog("Visual JSON Builder: Failed to load sequence from model.", "#F44336");}});
    m_dockBuilder = new QDockWidget(tr("Visual JSON"), this);
    m_dockBuilder->setObjectName("dockBuilder");
    m_dockBuilder->setWidget(m_swipeBuilder);
    m_dockBuilder->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_dockBuilder);
    m_dockBuilder->close();
    
    connect(m_dockBuilder, &QDockWidget::visibilityChanged, this, [this](bool visible){
        if (visible) m_swipeBuilder->startMonitoring();
        else m_swipeBuilder->stopMonitoring();});
    connect(m_swipeBuilder, &SwipeBuilderWidget::sequenceGenerated, this, [this](const QString &path){
        appendLog(QString("Sequence saved to JSON: %1. Use 'Load Sequence' to view in Runner.").arg(QFileInfo(path).fileName()), "#4CAF50");});
    // Menu Widoku
    m_viewCategoriesAct = m_dockCategories->toggleViewAction();
    m_viewCommandsAct = m_dockCommands->toggleViewAction();
    m_viewLogAct = m_dockLog->toggleViewAction();
    m_viewControlsAct = m_dockControls->toggleViewAction();
    m_viewSequenceAct = m_dockSequence->toggleViewAction();
    m_viewBuilderAct = m_dockBuilder->toggleViewAction();
    QMenu *viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction(m_viewCategoriesAct);
    viewMenu->addAction(m_viewCommandsAct);
    viewMenu->addAction(m_viewControlsAct);
    viewMenu->addAction(m_viewSequenceAct);
    viewMenu->addAction(m_viewBuilderAct);
    viewMenu->addAction(m_viewLogAct);
    // Detached Log Dialog
    LogDialog *dlg = new LogDialog(this);
    dlg->setDocument(m_log->document());
    dlg->move(this->x() + this->width() + 20, this->y());
    dlg->show();
    m_detachedLogDialog = dlg;
    loadCommands();
    populateCategoryList();
    if (m_categoryList->count() > 0) m_categoryList->setCurrentRow(0);
    if (!m_settings.contains("windowState")) { restoreDefaultLayout(); }
    restoreWindowStateFromSettings();
    m_commandView->installEventFilter(this);
    m_categoryList->installEventFilter(this);
    if (m_commandEdit) m_commandEdit->installEventFilter(this);
    refreshDeviceList();}

MainWindow::~MainWindow() {
    if (m_commandTimer && m_commandTimer->isActive()) m_commandTimer->stop();
    if (m_sequenceIntervalTimer && m_sequenceIntervalTimer->isActive()) m_sequenceIntervalTimer->stop();
    if (m_displayTimer && m_displayTimer->isActive()) m_displayTimer->stop();
    saveCommands();
    saveWindowStateToSettings();}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();}}

void MainWindow::dropEvent(QDropEvent *event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (urls.isEmpty()) return;
    QStringList jsonFiles;
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        if (path.endsWith(".json", Qt::CaseInsensitive)) {
            jsonFiles.append(path);}}
    if (!jsonFiles.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("Files detected");
        msgBox.setText("JSON files detected.");
        msgBox.setInformativeText("Load as Command List or Sequence?");
        QPushButton *cmdBtn = msgBox.addButton("Commands", QMessageBox::ActionRole);
        QPushButton *seqBtn = msgBox.addButton("Sequence", QMessageBox::ActionRole);
        msgBox.addButton(QMessageBox::Cancel);
        msgBox.exec();
        if (msgBox.clickedButton() == cmdBtn) {
            m_jsonFile = jsonFiles.first();
            loadCommands();
            populateCategoryList();
            appendLog(QString("Loaded commands from: %1").arg(m_jsonFile), "#4CAF50");
        } else if (msgBox.clickedButton() == seqBtn) {
            m_sequenceRunner->clearSequence();
            int count = 0;
            for (const QString &fn : jsonFiles) {
                if(m_sequenceRunner->appendSequence(fn)) count++;}
            appendLog(QString("Loaded sequence from %1 files via Drag&Drop.").arg(count), "#4CAF50");
            if (m_dockSequence) { m_dockSequence->setVisible(true); m_dockSequence->raise(); }}}}

void MainWindow::setupMenus() {
    QMenu *file = menuBar()->addMenu("&File");
    QAction *addAct = file->addAction("Add Command...");
    addAct->setShortcut(QKeySequence(tr("Ctrl+A")));
    connect(addAct, &QAction::triggered, this, &MainWindow::addCommand);
    QAction *editAct = file->addAction("Edit Command...");
    editAct->setShortcut(QKeySequence(tr("Ctrl+E")));
    connect(editAct, &QAction::triggered, this, &MainWindow::editCommand);
    QAction *removeAct = file->addAction("Remove Command");
    removeAct->setShortcut(QKeySequence(tr("Ctrl+R")));
    connect(removeAct, &QAction::triggered, this, &MainWindow::removeCommand);
    file->addSeparator();
    QAction *loadAct = file->addAction("Load commands…");
    QAction *saveAct = file->addAction("Save commands as…");
    file->addSeparator();
    QAction *quitAct = file->addAction("Quit");    
    connect(loadAct, &QAction::triggered, this, [this]{
        QDir startDir = QDir("/usr/local/etc/adb_shell");
        QString fn = QFileDialog::getOpenFileName(
                                             this,
                                             tr("Load JSON"),
                                             startDir.path(),
                                             tr("JSON files (*.json);;All files (*)"));
        if (!fn.isEmpty()) {
            m_jsonFile = fn;
            loadCommands();
            populateCategoryList();}});
    connect(saveAct, &QAction::triggered, this, [this]{
        QDir startDir = QDir("/usr/local/etc/adb_shell");
        QString fn = QFileDialog::getSaveFileName(
                                             this,
                                             tr("Save JSON"),
                                             startDir.filePath("adb_commands.json"),
                                             tr("JSON files (*.json);;All files (*)"));
        if (!fn.isEmpty()) {
            m_jsonFile = fn;
            saveCommands();}});
    connect(quitAct, &QAction::triggered, this, &QMainWindow::close);    
    QMenu *proc = menuBar()->addMenu("&Process");
    QAction *stopAct = proc->addAction("Stop current command");
    connect(stopAct, &QAction::triggered, this, &MainWindow::stopCommand);    
    QMenu *settings = menuBar()->addMenu("&Settings");
    QAction *restoreAct = settings->addAction("Restore default layout");
    connect(restoreAct, &QAction::triggered, this, &MainWindow::restoreDefaultLayout);
    QAction *appSettingsAct = settings->addAction("Application settings...");
    connect(appSettingsAct, &QAction::triggered, this, &MainWindow::showSettingsDialog);}

void MainWindow::ensureJsonPathLocal() {
    m_jsonFile = QDir("/usr/local/etc/adb_shell").filePath("adb_commands.json");}

QWidget* MainWindow::createSequenceWidget() {
    QWidget *sequenceRunnerWidget = new QWidget();
    auto mainLayout = new QVBoxLayout(sequenceRunnerWidget);
    mainLayout->setContentsMargins(5, 5, 5, 5);     
    auto loadLayout = new QHBoxLayout();
    QPushButton *loadBtn = new QPushButton("Load Sequence...");
    connect(loadBtn, &QPushButton::clicked, this, &MainWindow::loadSequenceFile);
    loadLayout->addWidget(loadBtn);
    QPushButton *previewBtn = new QPushButton("Preview");
    previewBtn->setStyleSheet("background-color: #2196F3; color: black;");
    connect(previewBtn, &QPushButton::clicked, this, &MainWindow::showSequencePreview);
    loadLayout->addWidget(previewBtn);
    QPushButton *runBtn = new QPushButton("Run Sequence");
    runBtn->setStyleSheet("background-color: #4CAF50; color: black;");
    connect(runBtn, &QPushButton::clicked, m_sequenceRunner, &SequenceRunner::startSequence);
    loadLayout->addWidget(runBtn);
    QPushButton *stopBtn = new QPushButton("Stop Sequence");
    stopBtn->setStyleSheet("background-color: #FF0000; color: black;");
    connect(stopBtn, &QPushButton::clicked, m_sequenceRunner, &SequenceRunner::stopSequence);
    loadLayout->addWidget(stopBtn);
    mainLayout->addLayout(loadLayout);
    QHBoxLayout *intervalLayout = new QHBoxLayout();
    m_sequenceIntervalToggle = new QCheckBox("Interval (s):");
    connect(m_sequenceIntervalToggle, &QCheckBox::toggled, m_sequenceRunner, &SequenceRunner::setIntervalToggle);
    intervalLayout->addWidget(m_sequenceIntervalToggle);
    m_sequenceIntervalSpinBox = new QSpinBox();
    m_sequenceIntervalSpinBox->setRange(1, 86400);
    m_sequenceIntervalSpinBox->setValue(60);
    m_sequenceIntervalSpinBox->setMaximumWidth(70);
    connect(m_sequenceIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
                m_sequenceRunner, &SequenceRunner::setIntervalValue);
    intervalLayout->addWidget(m_sequenceIntervalSpinBox);      
    m_sequenceIntervalLabel = new QLabel("Wait: 0s");
    m_sequenceIntervalLabel->setStyleSheet("color: black;");
    intervalLayout->addWidget(m_sequenceIntervalLabel);
    intervalLayout->addStretch(1);      
    mainLayout->addLayout(intervalLayout);
    mainLayout->addStretch(1);
    return sequenceRunnerWidget;}

void MainWindow::showSequencePreview() {
    QStringList commands = m_sequenceRunner->getCommandsAsText();
    QDialog dialog(this);
    dialog.setWindowTitle("Current Workflow Queue");
    dialog.resize(600, 400);    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QTextEdit *textEdit = new QTextEdit();
    textEdit->setReadOnly(true);
    textEdit->setFontFamily("Monospace");    
    QString content = QString("--- TOTAL COMMANDS: %1 ---\n\n").arg(commands.count());
    if (commands.isEmpty()) {
        content += "No commands loaded in queue.";
    } else {
        for (int i = 0; i < commands.count(); ++i) {
            content += QString("[%1] %2\n").arg(i + 1, 2, 10, QChar('0')).arg(commands.at(i));}}
    textEdit->setText(content);
    layout->addWidget(textEdit);    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *clearBtn = new QPushButton("Clear Queue");
    clearBtn->setStyleSheet("background-color: #F44336; color: white;");
    connect(clearBtn, &QPushButton::clicked, [this, &dialog, &textEdit](){
        m_sequenceRunner->clearSequence();
        textEdit->setText("Queue cleared.");});
    btnLayout->addWidget(clearBtn);
    QPushButton *closeButton = new QPushButton("Close");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    btnLayout->addWidget(closeButton);
    layout->addLayout(btnLayout);
    dialog.exec();}

void MainWindow::setupSequenceDock() {
    m_dockSequence = new QDockWidget(tr("Sequence"), this);
    m_dockSequence->setObjectName("dockSequence");
    m_dockSequence->setWidget(createSequenceWidget());
    m_dockSequence->setAllowedAreas(Qt::TopDockWidgetArea | Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea | Qt::BottomDockWidgetArea);
    addDockWidget(Qt::TopDockWidgetArea, m_dockSequence);
    splitDockWidget(m_dockControls, m_dockSequence, Qt::Horizontal);}

QWidget* MainWindow::createControlsWidget() {
    QWidget *w = new QWidget();
    w->setMinimumHeight(90); 
    w->setMinimumWidth(350);
    auto mainLayout = new QVBoxLayout(w);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    m_commandEdit = new QLineEdit();
    connect(m_commandEdit, &QLineEdit::returnPressed, this, &MainWindow::runCommand);
    mainLayout->addWidget(m_commandEdit);
    auto mixedLayout = new QHBoxLayout();
    m_deviceCombo = new QComboBox();
    m_deviceCombo->setMinimumWidth(90); 
    m_deviceCombo->addItem("Detecting...");
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceSelected);
    mixedLayout->addWidget(m_deviceCombo, 1);
    m_refreshDevicesBtn = new QPushButton("↻"); 
    m_refreshDevicesBtn->setMaximumWidth(30);
    connect(m_refreshDevicesBtn, &QPushButton::clicked, this, &MainWindow::refreshDeviceList);
    mixedLayout->addWidget(m_refreshDevicesBtn);
    mixedLayout->addSpacing(10);
    m_intervalSpinBox = new QSpinBox();
    m_intervalSpinBox->setRange(1, 86400);
    m_intervalSpinBox->setValue(5);
    m_intervalSpinBox->setMaximumWidth(60);
    mixedLayout->addWidget(m_intervalSpinBox);
    m_intervalToggle = new QCheckBox("Interval");
    m_intervalToggle->setChecked(false);
    mixedLayout->addWidget(m_intervalToggle);    
    m_commandTimerLabel = new QLabel("(0s)");
    m_commandTimerLabel->setStyleSheet("color: black; font-weight: normal;");
    mixedLayout->addWidget(m_commandTimerLabel);
    m_scheduleBtn = new QPushButton("Schedule");
    m_scheduleBtn->setMaximumWidth(70);
    m_scheduleBtn->setStyleSheet("background-color: #FFAA00; color: black;");
    connect(m_scheduleBtn, &QPushButton::clicked, this, &MainWindow::onScheduleButtonClicked);
    mixedLayout->addWidget(m_scheduleBtn);    
    QPushButton *stopTimerBtn = new QPushButton("Stop");
    stopTimerBtn->setMaximumWidth(50);
    stopTimerBtn->setStyleSheet("background-color: #FF0000; color: black;");
    connect(stopTimerBtn, &QPushButton::clicked, [this]{
        if (m_commandTimer->isActive()) {
            m_commandTimer->stop();
            m_intervalToggle->setChecked(false);
            appendLog("Timer stopped.", "#E68D8D");}});
    mixedLayout->addWidget(stopTimerBtn);
    mainLayout->addLayout(mixedLayout);
    auto btnLayout = new QHBoxLayout();    
    m_rootToggle = new QCheckBox("root");
    m_rootToggle->setChecked(m_isRootShell);
    connect(m_rootToggle, &QCheckBox::checkStateChanged, this, [this](int state) {
        m_isRootShell = (state == Qt::Checked);
        m_settings.setValue("isRootShell", m_isRootShell);});
    btnLayout->addWidget(m_rootToggle);    
    m_shellToggle = new QCheckBox("shell");
    m_shellToggle->setChecked(m_settings.value("isShellCommand", false).toBool());
    connect(m_shellToggle, &QCheckBox::checkStateChanged, this, [this](int state) {
        m_settings.setValue("isShellCommand", (state == Qt::Checked));});
    btnLayout->addWidget(m_shellToggle);    
    btnLayout->addSpacing(20);
    m_runBtn = new QPushButton("Execute");
    connect(m_runBtn, &QPushButton::clicked, this, &MainWindow::runCommand);
    btnLayout->addWidget(m_runBtn);    
    m_stopBtn = new QPushButton("Stop Process");
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::stopCommand);
    btnLayout->addWidget(m_stopBtn);    
    m_clearBtn = new QPushButton("Clear Log");
    connect(m_clearBtn, &QPushButton::clicked, [this](){ m_log->clear(); m_detachedLogDialog->clear(); });
    btnLayout->addWidget(m_clearBtn);    
    m_saveBtn = new QPushButton("Save Log");
    connect(m_saveBtn, &QPushButton::clicked, [this](){
        QString defDir = "/usr/local/log";
        QDir d(defDir);
        if (!d.exists()) QDir().mkpath(defDir);
        QString fn = QFileDialog::getSaveFileName(this, "Save log", d.filePath("log.txt"));
        if (!fn.isEmpty()) {
            QFile f(fn);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(m_log->toPlainText().toUtf8());
                f.close();}}});
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);
    return w;}


void MainWindow::loadSequenceFile() {
    QDir startDir = QDir("/usr/local/etc/sequencerunner/");
    QStringList fns = QFileDialog::getOpenFileNames(
                                             this,
                                             tr("Load Sequence JSON(s)"),
                                             startDir.path(),
                                             tr("JSON files (*.json);;All files (*)"));
    if (!fns.isEmpty()) {
        m_sequenceRunner->clearSequence();
        int successCount = 0;
        for (const QString &fn : fns) {
             if (m_sequenceRunner->appendSequence(fn)) successCount++;}        
        if (successCount > 0) {
             appendLog(QString("Sequence queue loaded from %1 files.").arg(successCount), "#8BC34A");
             if (m_dockSequence) { m_dockSequence->setVisible(true); m_dockSequence->raise(); }
        } else {
             appendLog("Error loading sequence files.", "#F44336");}}}


void MainWindow::onSequenceStarted() {
    appendLog("--- HEADLESS SEQUENCE STARTED ---", "#4CAF50");
    if (m_dockSequence) {
        m_dockSequence->setVisible(true);
        m_dockSequence->raise();
    }
    // FIX 1: Wyłączenie przycisku uruchamiania sekwencji w Visual JSON (SwipeBuilder)
    if (m_swipeBuilder) {
        // WYMAGA DODANIA: void SwipeBuilderWidget::setRunSequenceButtonEnabled(bool enabled);
        m_swipeBuilder->setRunSequenceButtonEnabled(false);
    }
}

void MainWindow::onSequenceFinished(bool success) {
    if (success) {
        appendLog("--- HEADLESS SEQUENCE FINISHED SUCCESSFULLY ---", "#4CAF50");
    } else {
        appendLog("--- HEADLESS SEQUENCE TERMINATED WITH ERROR ---", "#F44336");
    }
    if (m_swipeBuilder) {
        m_swipeBuilder->setRunSequenceButtonEnabled(true);}}

void MainWindow::onSequenceCommandExecuting(const QString &cmd, int index, int total) {
    appendLog(QString(">> HEADLESS [%1/%2]: %3").arg(index).arg(total).arg(cmd), "#00BCD4");}

void MainWindow::handleSequenceLog(const QString &text, const QString &color) {appendLog(text, color);}


void MainWindow::onScheduleButtonClicked() {
    const QString cmdText = m_commandEdit->text().trimmed();
    const int interval = m_intervalSpinBox->value();
    const bool periodic = m_intervalToggle->isChecked();    
    if (cmdText.isEmpty()) {
        QMessageBox::warning(this, "Schedule Error", "Command cannot be empty.");
        return;}
    bool safeMode = m_settings.value("safeMode", false).toBool();
    if (safeMode && isDestructiveCommand(cmdText)) {
        QMessageBox::warning(this, "Safe mode", "Application is in Safe Mode. Destructive commands are blocked from scheduling.");
        return;}
    if (m_commandTimer->isActive()) {
        m_commandTimer->stop();
        appendLog("Previous scheduled command canceled.", "#FFAA66");}
    m_scheduledCommand = cmdText;
    m_commandTimer->setSingleShot(!periodic);
    m_commandTimer->setInterval(interval * 1000);    
    if (periodic) {
        appendLog(QString("Scheduled periodic command: %1 (every %2 s)").arg(cmdText).arg(interval), "#4CAF50");
    } else {
        appendLog(QString("Scheduled one-shot command: %1 (in %2 s)").arg(cmdText).arg(interval), "#00BCD4");}
    m_commandTimer->start();}


void MainWindow::executeScheduledCommand() {
    const QString cmdToRun = m_scheduledCommand;
    if (m_commandTimer->isSingleShot()) {
        m_commandTimer->stop();}
    m_commandEdit->setText(cmdToRun);
    runCommand();}

void MainWindow::loadCommands() {
    m_commands.clear();
    std::ifstream ifs(m_jsonFile.toStdString());
    if (!ifs.is_open()) {
        QStringList cats = {"System", "shell", "config"};
        for (const QString &c: cats) m_commands.insert(c, {});
        saveCommands();
        return;}
    try {
        nlohmann::json j;
        ifs >> j;
        for (auto it = j.begin(); it != j.end(); ++it) {
            const QString category = QString::fromStdString(it.key());
            QVector<SystemCmd> vec;
            if (it.value().is_array()) {
                for (const auto& cmd_obj : it.value()) {
                    if (cmd_obj.is_object() && cmd_obj.contains("command") && cmd_obj.contains("description")) {
                        SystemCmd c;
                        c.command = QString::fromStdString(cmd_obj.at("command").get<std::string>());
                        c.description = QString::fromStdString(cmd_obj.at("description").get<std::string>());
                        vec.append(c);}}}
            m_commands.insert(category, vec);}
    } catch (const nlohmann::json::exception& e) {
        QMessageBox::warning(this, "Error JSON", QString("Cannot parse JSON file: %1\nError: %2").arg(m_jsonFile).arg(e.what()));
        QStringList cats = { "System", "shell", "config" };
        for (const QString &c: cats) m_commands.insert(c, {});
        return;}}

void MainWindow::saveCommands() {
    nlohmann::json j_root = nlohmann::json::object();
    for (auto it = m_commands.begin(); it != m_commands.end(); ++it) {
        nlohmann::json j_array = nlohmann::json::array();
        for (const SystemCmd &c: it.value()) {
            j_array.push_back({
                {"command", c.command.toStdString()},
                {"description", c.description.toStdString()}});}
        j_root[it.key().toStdString()] = j_array;}
    QFileInfo fileInfo(m_jsonFile);
    QDir dir;
    if (!dir.mkpath(fileInfo.absolutePath())) {
        QMessageBox::critical(this, "Error Save JSON", QString("Cannot create directory: %1").arg(fileInfo.absolutePath()));
        return;}
    try {
        std::ofstream ofs(m_jsonFile.toStdString());
        if (ofs.is_open()) {
            ofs << j_root.dump(4);
            ofs.close();
        } else {
            QMessageBox::critical(this, "Error Save JSON", QString("Cannot write JSON: %1").arg(m_jsonFile));}
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error JSON Save", QString("Cannot save JSON file: %1").arg(e.what()));}}

void MainWindow::populateCategoryList() {
    m_categoryList->clear();
    for (auto it = m_commands.constBegin(); it != m_commands.constEnd(); ++it) m_categoryList->addItem(it.key());}

void MainWindow::populateCommandList(const QString &category) {
    m_commandModel->removeRows(0, m_commandModel->rowCount());
    auto vec = m_commands.value(category);
    for (const SystemCmd &c: vec) {
        QList<QStandardItem*> row;
        row << new QStandardItem(c.command) << new QStandardItem(c.description);
        m_commandModel->appendRow(row);}
    m_commandView->resizeColumnToContents(1);}

void MainWindow::onCategoryChanged(QListWidgetItem *current, QListWidgetItem *) {
    if (!current) return;
    populateCommandList(current->text());
    m_commandEdit->clear();
    m_inputHistoryIndex = -1;}

void MainWindow::onCommandSelected(const QModelIndex &current, const QModelIndex &) {
    if (!current.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(current);
    if (s.isValid()) {
        QString cmd = m_commandModel->item(s.row(), 0)->text();
        m_commandEdit->setText(cmd);
        if (m_dockControls) { 
            m_dockControls->setVisible(true); m_dockControls->raise(); 
            if (m_commandEdit) { m_commandEdit->setFocus(); m_commandEdit->selectAll(); } }}}

void MainWindow::onCommandDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(index);
    if (s.isValid()) {
        QString cmd = m_commandModel->item(s.row(), 0)->text();
        m_commandEdit->setText(cmd);
        runCommand();}}

void MainWindow::runCommand() {
    const QString cmdText = m_commandEdit->text().trimmed();
    if (cmdText.isEmpty()) return;
    bool safeMode = m_settings.value("safeMode", false).toBool();
    if (safeMode && isDestructiveCommand(cmdText)) {
        QMessageBox::warning(this, "Safe mode", "Application is in Safe Mode. Destructive commands are blocked.");
        return;}
    if (isDestructiveCommand(cmdText)) {
        auto reply = QMessageBox::question(this, "Confirm", QString("Command looks destructive:\n%1\nContinue?").arg(cmdText), QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;}
    if (m_inputHistory.isEmpty() || m_inputHistory.last() != cmdText) m_inputHistory.append(cmdText);
    m_inputHistoryIndex = -1;    
    QStringList args;
    const bool isShell = m_shellToggle->isChecked();
    const bool isRoot = m_rootToggle->isChecked();    
    QStringList parsedCmd = ArgsParser::parse(cmdText);
    if (isRoot) {
        args << "shell" << "su" << "-c" << cmdText;
        appendLog(QString(">>> adb root: %1").arg(cmdText), "#FF0000");
    } else if (isShell) {
        args.append("shell");
        args.append(parsedCmd);
        appendLog(QString(">>> adb shell: %1").arg(cmdText), "#00BCD4");
    } else {
        args = parsedCmd;
        appendLog(QString(">>> adb: %1").arg(cmdText), "#FFE066");}
    m_executor->runAdbCommand(args);}

void MainWindow::stopCommand() {
    if (m_executor) {
        m_executor->stop();
        appendLog("Process stopped by user (adb terminated).", "#FFAA66");}}

void MainWindow::onOutput(const QString &text) {
    const QStringList lines = text.split('\n');
    for (const QString &l : lines) {
        if (!l.trimmed().isEmpty()) {
            appendLog(l.trimmed(), "#A9FFAC");}}}

void MainWindow::onError(const QString &text) {
    const QStringList lines = text.split('\n');
    for (const QString &l : lines) {
        if (!l.trimmed().isEmpty()) {
            appendLog(QString("!!! %1").arg(l.trimmed()), "#FF6565");}}
    logErrorToFile(text);}

void MainWindow::onProcessStarted() { appendLog("adb command started.", "#8ECAE6"); }

void MainWindow::addCommand() {
    bool ok;
    QString cmd = QInputDialog::getText(this, "Add command", "Command (e.g., shell ls -l or reboot):", QLineEdit::Normal, "", &ok);
    if (!ok || cmd.isEmpty()) return;
    QString desc = QInputDialog::getText(this, "Add command", "Description:", QLineEdit::Normal, "", &ok);
    if (!ok) return;
    QString category = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : QString();
    if (category.isEmpty()) { QMessageBox::warning(this, "No category", "Select a category first."); return; }
    m_commands[category].append({cmd, desc});
    populateCommandList(category);
    saveCommands();}

void MainWindow::editCommand() {
    QModelIndex idx = m_commandView->currentIndex();
    if (!idx.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(idx);
    if (!s.isValid()) return;
    QString cmd = m_commandModel->item(s.row(), 0)->text();
    QString desc = m_commandModel->item(s.row(), 1)->text();
    bool ok;
    QString ncmd = QInputDialog::getText(this, "Edit command", "Command (e.g., shell ls -l or reboot):", QLineEdit::Normal, cmd, &ok);
    if (!ok) return;
    QString ndesc = QInputDialog::getText(this, "Edit command", "Description:", QLineEdit::Normal, desc, &ok);
    if (!ok) return;
    QString category = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : QString();
    if (category.isEmpty()) return;
    auto &vec = m_commands[category];
    for (int i=0; i<vec.size(); ++i) {
        if (vec[i].command == cmd && vec[i].description == desc) { 
            vec[i].command = ncmd; 
            vec[i].description = ndesc; 
            break; }}
    populateCommandList(category);
    saveCommands();}

void MainWindow::removeCommand() {
    QModelIndex idx = m_commandView->currentIndex();
    if (!idx.isValid()) return;
    QModelIndex s = m_commandProxy->mapToSource(idx);
    if (!s.isValid()) return;
    QString cmd = m_commandModel->item(s.row(), 0)->text();
    QString category = m_categoryList->currentItem() ? m_categoryList->currentItem()->text() : QString();
    if (category.isEmpty()) return;
    auto &vec = m_commands[category];
    for (int i=0; i<vec.size(); ++i) {
        if (vec[i].command == cmd) { vec.removeAt(i); break; }}
    populateCommandList(category);
    saveCommands();}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus) {
    appendLog(QString("adb process finished. Exit code: %1").arg(exitCode), "#BDBDBD");
    if (exitCode != 0) appendLog(QString("Command finished with error code: %1").arg(exitCode), "#FF6565");}

void MainWindow::appendLog(const QString &text, const QString &color) {
    QString line = text;
    if (!color.isEmpty()) m_log->setTextColor(QColor(color)); else m_log->setTextColor(QColor("#F0F0F0"));
    m_log->append(line);
    m_log->setTextColor(QColor("#F0F0F0"));}

void MainWindow::logErrorToFile(const QString &text) {
    const QString logDir = "/usr/local/log";
    QDir d(logDir);
    if (!d.exists()) QDir().mkpath(logDir);
    QString logFile = d.filePath("adb_shell.log");
    QFile f(logFile);
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << " - " << text << "\n";
        f.close();}}

bool MainWindow::isDestructiveCommand(const QString &cmd) {
    QString c = cmd.toLower();
    return c.contains("rm ") || c.contains("wipe") || c.contains("format") || c.contains("dd ") || c.contains("flashall");}

void MainWindow::navigateHistory(int direction) {
    if (m_inputHistory.isEmpty()) {return;}
    if (m_inputHistoryIndex == -1) {
    if (direction == -1) {m_inputHistoryIndex = m_inputHistory.size() - 1;} else {return;}} else {m_inputHistoryIndex += direction;
    if (m_inputHistoryIndex < 0) {m_inputHistoryIndex = 0;return;}
    if (m_inputHistoryIndex >= m_inputHistory.size()) {m_commandEdit->clear();m_inputHistoryIndex = -1;return;}}
    m_commandEdit->setText(m_inputHistory.at(m_inputHistoryIndex));
    m_commandEdit->selectAll();}

void MainWindow::restoreDefaultLayout() {
    m_settings.remove("geometry");
    m_settings.remove("windowState");
    addDockWidget(Qt::TopDockWidgetArea, m_dockControls);
    addDockWidget(Qt::TopDockWidgetArea, m_dockSequence);
    splitDockWidget(m_dockControls, m_dockSequence, Qt::Horizontal);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCategories);
    addDockWidget(Qt::LeftDockWidgetArea, m_dockCommands);
    splitDockWidget(m_dockCategories, m_dockCommands, Qt::Horizontal);
    addDockWidget(Qt::BottomDockWidgetArea, m_dockLog);
    saveWindowStateToSettings();}

void MainWindow::showSettingsDialog() {
    SettingsDialog dlg(this);
    dlg.setSafeMode(m_settings.value("safeMode", false).toBool());
    if (dlg.exec() == QDialog::Accepted) {
        m_settings.setValue("safeMode", dlg.safeMode());}}

void MainWindow::restoreWindowStateFromSettings() {
    if (m_settings.contains("geometry")) restoreGeometry(m_settings.value("geometry").toByteArray());
    if (m_settings.contains("windowState")) restoreState(m_settings.value("windowState").toByteArray());
    m_isRootShell = m_settings.value("isRootShell", false).toBool();
    if (m_rootToggle) m_rootToggle->setChecked(m_isRootShell);}

void MainWindow::saveWindowStateToSettings() {
    m_settings.setValue("geometry", saveGeometry());
    m_settings.setValue("windowState", saveState());
    m_settings.setValue("isRootShell", m_isRootShell);}

QModelIndex MainWindow::currentCommandModelIndex() const {
    QModelIndex idx = m_commandView->currentIndex();
    if (!idx.isValid()) return QModelIndex();
    return m_commandProxy->mapToSource(idx);}


bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if ((obj == m_commandView || obj == m_categoryList || obj == m_commandEdit) && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        
        if (obj == m_commandEdit) {
            if (ke->key() == Qt::Key_Up) {
                navigateHistory(-1);
                return true;
            }
            if (ke->key() == Qt::Key_Down) {
                navigateHistory(1);
                return true;
            }
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (obj == m_categoryList) {
                if (m_commandModel->rowCount() > 0) {
                    QModelIndex firstIndex = m_commandView->model()->index(0, 0);
                    if (firstIndex.isValid()) {
                        m_commandView->setCurrentIndex(firstIndex);
                        m_commandView->setFocus();
                        return true;
                    }
                }
                if (m_commandEdit) {
                    m_commandEdit->setFocus();
                    return true;
                }
            }
            if (obj == m_commandView) {
                onCommandDoubleClicked(m_commandView->currentIndex());
                return true;
            }
            // Zezwolenie, aby QLineEdit przetworzyło Enter (wywoła runCommand)
            if (obj == m_commandEdit) {
                return false;
            }
        }
        // Jeżeli to KeyPress, ale nie jest to klawisz używany do nawigacji,
        // domyślnie zwróć false, aby event mógł być przetworzony dalej.
        return false;}
    return QMainWindow::eventFilter(obj, event);}

void MainWindow::startIntervalSequence() {
    appendLog("--- INTERVAL SEQUENCE RESTARTING ---", "#4CAF50");
    m_sequenceRunner->startSequence();}


void MainWindow::closeEvent(QCloseEvent *event) {
    saveWindowStateToSettings();
    QMainWindow::closeEvent(event);}


void MainWindow::updateTimerDisplay() {
    if (m_commandTimer->isActive()) {
        qint64 remaining = m_commandTimer->remainingTime();
        if (remaining < 0) remaining = 0;
        m_commandTimerLabel->setText(QString("(%1 s)").arg(remaining / 1000));
    } else {
        m_commandTimerLabel->setText("(0 s)");}
    if (m_sequenceIntervalTimer->isActive()) {
        qint64 remaining = m_sequenceIntervalTimer->remainingTime();
        if (remaining < 0) remaining = 0;
        m_sequenceIntervalLabel->setText(QString("Next run in: %1 s").arg(remaining / 1000));
    } else {
        m_sequenceIntervalLabel->setText("Wait: 0s");}}


void MainWindow::refreshDeviceList() {
    m_deviceCombo->clear();
    m_deviceCombo->addItem("Searching...");
    QProcess *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int exitCode, QProcess::ExitStatus) {
        m_deviceCombo->clear();
        if (exitCode == 0) {
            QString output = p->readAllStandardOutput();
            QStringList lines = output.split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                if (line.startsWith("List of")) continue;
                if (line.trimmed().isEmpty()) continue;
                QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QString serial = parts[0];
                    QString status = parts[1];
                    m_deviceCombo->addItem(QString("%1 (%2)").arg(serial, status), serial);}}}
        if (m_deviceCombo->count() == 0) {
            m_deviceCombo->addItem("No devices found");
        } else {
            onDeviceSelected(0);}
        p->deleteLater();});
    p->start(m_executor->adbPath(), QStringList() << "devices");}

void MainWindow::onDeviceSelected(int index) {
    if (index < 0 || index >= m_deviceCombo->count()) return;
    QString serial = m_deviceCombo->itemData(index).toString();
    if (!serial.isEmpty()) {
        m_executor->setTargetDevice(serial);
        appendLog(QString("Target device set to: %1").arg(serial), "#2196F3");
        if (m_swipeBuilder) {
            m_swipeBuilder->setAdbPath(m_executor->adbPath());}
    } else {
        m_executor->setTargetDevice("");}}

```

```
//mainwindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "systemcmd.h"
#include "SwipeBuilderWidget.h"
#include <QMainWindow>
#include <QMap>
#include <QVector>
#include <QString>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QDir>
#include <QComboBox>

class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QTextEdit;
class QDockWidget;
class QTreeView;
class QStandardItemModel;
class QSortFilterProxyModel;
class QPushButton;
class CommandExecutor;
class QAction;
class LogDialog;
class SequenceRunner;
class QSpinBox;
class QCheckBox;
class QModelIndex;
class QHBoxLayout;
class QLabel;
class QDragEnterEvent;
class QDropEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr, 
                        const QString &adbPath = QString(), 
                        const QString &targetSerial = QString());
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void onCategoryChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void onCommandSelected(const QModelIndex &current, const QModelIndex &previous);
    void onCommandDoubleClicked(const QModelIndex &index);
    void runCommand();
    void stopCommand();
    void addCommand();
    void editCommand();
    void removeCommand();
    void saveCommands();
    void loadCommands();
    void onOutput(const QString &text);
    void onError(const QString &text);
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void restoreDefaultLayout();
    void showSettingsDialog();
    void navigateHistory(int direction);    
    void onScheduleButtonClicked();
    void executeScheduledCommand();
    void updateTimerDisplay();
    void loadSequenceFile();
    void startIntervalSequence();
    void onSequenceStarted();
    void onSequenceFinished(bool success);
    void onSequenceCommandExecuting(const QString &cmd, int index, int total);
    void handleSequenceLog(const QString &text, const QString &color);    
    void showSequencePreview();
    void refreshDeviceList(); 
    void onDeviceSelected(int index);

private:
    QDockWidget *m_dockBuilder = nullptr;
    SwipeBuilderWidget *m_swipeBuilder = nullptr;
    QAction *m_viewBuilderAct = nullptr;
    QDockWidget *m_dockCategories = nullptr;
    QDockWidget *m_dockCommands = nullptr;
    QDockWidget *m_dockLog = nullptr;
    QDockWidget *m_dockControls = nullptr;
    QDockWidget *m_dockSequence = nullptr;
    QListWidget *m_categoryList = nullptr;
    QTreeView *m_commandView = nullptr;
    QStandardItemModel *m_commandModel = nullptr;
    QSortFilterProxyModel *m_commandProxy = nullptr;
    QLineEdit *m_commandEdit = nullptr;
    QTextEdit *m_log = nullptr;
    QPushButton *m_runBtn = nullptr;
    QPushButton *m_stopBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_saveBtn = nullptr;
    QPushButton *m_scheduleBtn = nullptr;
    QComboBox *m_deviceCombo = nullptr;
    QPushButton *m_refreshDevicesBtn = nullptr;
    QMap<QString, QVector<SystemCmd>> m_commands;
    CommandExecutor *m_executor = nullptr;
    QString m_adbPath = QStringLiteral("adb");
    QString m_jsonFile = QStringLiteral("adb_commands.json");
    QStringList m_inputHistory;
    int m_inputHistoryIndex = -1;    
    QCheckBox *m_shellToggle = nullptr;
    QCheckBox *m_rootToggle = nullptr;
    bool m_isRootShell = false;
    QTimer *m_commandTimer = nullptr;
    QLabel *m_commandTimerLabel = nullptr;
    QSpinBox *m_intervalSpinBox = nullptr;
    QCheckBox *m_intervalToggle = nullptr;
    QString m_scheduledCommand;
    SequenceRunner *m_sequenceRunner = nullptr;
    QCheckBox *m_sequenceIntervalToggle = nullptr;
    QSpinBox *m_sequenceIntervalSpinBox = nullptr;
    QTimer *m_sequenceIntervalTimer = nullptr;
    QLabel *m_sequenceIntervalLabel = nullptr;
    QVector<QString> m_sequenceQueue;
    QTimer *m_displayTimer = nullptr;
    QAction *m_addCommandAct = nullptr;
    QAction *m_editCommandAct = nullptr;
    QAction *m_removeCommandAct = nullptr;
    QAction *m_viewCategoriesAct = nullptr;
    QAction *m_viewCommandsAct = nullptr;
    QAction *m_viewLogAct = nullptr;
    QAction *m_viewControlsAct = nullptr;
    QAction *m_viewSequenceAct = nullptr;
    QSettings m_settings{"AdbShell", "adb_shell"};
    LogDialog *m_detachedLogDialog = nullptr;

    void populateCategoryList();
    void populateCommandList(const QString &category);
    void appendLog(const QString &text, const QString &color = QString());
    void logErrorToFile(const QString &text);
    bool isDestructiveCommand(const QString &cmd);
    QWidget* createControlsWidget();
    QWidget* createSequenceWidget();
    void setupSequenceDock();
    void ensureJsonPathLocal();
    void setupMenus();
    void restoreWindowStateFromSettings();
    void saveWindowStateToSettings();
    QModelIndex currentCommandModelIndex() const;
};

#endif // MAINWINDOW_H
```

```
//remoteserver.cpp
#include "remoteserver.h"
#include "commandexecutor.h"
#include "sequencerunner.h"
#include <QImage>
#include <QBuffer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QDateTime>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QHostAddress>
#include <QProcess>
#include <QTimer>

RemoteServer::RemoteServer(const QString &adbPath, const QString &targetSerial, quint16 port, QObject *parent) 
    : QObject(parent) {
    
    m_executor = new CommandExecutor(this);
    m_executor->setAdbPath(adbPath);
    if (!targetSerial.isEmpty()) {
        m_executor->setTargetDevice(targetSerial);}
    m_runner = new SequenceRunner(m_executor, this);
    connect(m_runner, &SequenceRunner::logMessage, this, &RemoteServer::onRunnerLog);
    connect(m_runner, &SequenceRunner::sequenceFinished, this, &RemoteServer::onRunnerFinished);
    m_screenProcess = new QProcess(this);
    connect(m_screenProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RemoteServer::onScreenCaptured);
    m_screenTimer = new QTimer(this);
//Interval screenTimer refresh
    m_screenTimer->setInterval(33); 
    m_screenTimer->setSingleShot(false);
    connect(m_screenTimer, &QTimer::timeout, this, &RemoteServer::captureScreen);
    m_wsServer = new QWebSocketServer(QStringLiteral("AdbSequenceServer"), 
                                      QWebSocketServer::NonSecureMode, this);
    if (m_wsServer->listen(QHostAddress::Any, port)) {
        qDebug() << "Server started on ws://0.0.0.0:" << port;
        qDebug() << "Target ADB Path:" << m_executor->adbPath();
        qDebug() << "Target Device:" << (targetSerial.isEmpty() ? "AUTO/Any" : targetSerial);
        connect(m_wsServer, &QWebSocketServer::newConnection, this, &RemoteServer::onNewConnection);
    } else {
        qCritical() << "Error starting server:" << m_wsServer->errorString();}}


RemoteServer::~RemoteServer() {
    m_wsServer->close();
    if (m_screenTimer && m_screenTimer->isActive()) {
        m_screenTimer->stop();}
    qDeleteAll(m_clients.begin(), m_clients.end());}


void RemoteServer::onNewConnection() {
    QWebSocket *pSocket = m_wsServer->nextPendingConnection();
    qDebug() << "Client connected from" << pSocket->peerAddress().toString();
    connect(pSocket, &QWebSocket::textMessageReceived, this, &RemoteServer::onTextMessageReceived);
    connect(pSocket, &QWebSocket::disconnected, this, &RemoteServer::onSocketDisconnected);
    m_clients << pSocket;
    pSocket->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("connected"), QStringLiteral("Polaczono z serwerem AdbSequence."))).toJson(QJsonDocument::Compact));
    if (m_clients.size() == 1) {
        m_screenTimer->start();
        qDebug() << "Screen streaming started (10 FPS).";}}


void RemoteServer::onSocketDisconnected() {
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient) {
        qDebug() << "Client disconnected from" << pClient->peerAddress().toString();
        m_clients.removeAll(pClient);
        pClient->deleteLater();
        if (m_clients.isEmpty()) {
            m_screenTimer->stop();
            qDebug() << "Screen streaming stopped.";}}}


void RemoteServer::onTextMessageReceived(const QString &message) {
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (!pClient) return;
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        pClient->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Invalid JSON format."))).toJson(QJsonDocument::Compact));
        return;}
    handleCommand(pClient, doc.object());}


void RemoteServer::handleCommand(QWebSocket *sender, const QJsonObject &json) {
    if (!json.contains(QStringLiteral("command"))) {
        sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Missing 'command' field."))).toJson(QJsonDocument::Compact));
        return;}
    QString command = json[QStringLiteral("command")].toString();
    QJsonObject payload = json[QStringLiteral("payload")].toObject();
    if (command == QStringLiteral("loadSequence")) {
        if (!payload.contains(QStringLiteral("path"))) {
            sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Missing 'path' for loadSequence."))).toJson(QJsonDocument::Compact));
            return;}
        QString path = payload[QStringLiteral("path")].toString();
        if (m_runner->isRunning()) {
            sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Cannot load sequence while a sequence is running."))).toJson(QJsonDocument::Compact));
            return;}
        if (m_runner->appendSequence(path)) { 
            sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("ok"), QStringLiteral("Sekwencja zaladowana. Gotowa do startu."))).toJson(QJsonDocument::Compact));
        } else {
            sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Blad ladowania sekwencji. Sprawdz logi."))).toJson(QJsonDocument::Compact));}
    } else if (command == QStringLiteral("startSequence")) {
        if (m_runner->startSequence()) { 
            sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("ok"), QStringLiteral("Sekwencja uruchomiona."))).toJson(QJsonDocument::Compact));
        } else {
            sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Nie udalo sie uruchomic sekwencji. Upewnij sie ze zostala zaladowana."))).toJson(QJsonDocument::Compact));}
    } else if (command == QStringLiteral("stopSequence")) {
        m_runner->stopSequence();
        sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("ok"), QStringLiteral("Sekwencja zatrzymana."))).toJson(QJsonDocument::Compact));
    } else if (command == QStringLiteral("status")) {
        QString statusMsg = m_runner->isRunning() ? QStringLiteral("running") : QStringLiteral("idle");
        sender->sendTextMessage(QJsonDocument(createStatusMessage(statusMsg, QStringLiteral("Aktualny status SequenceRunnera."))).toJson(QJsonDocument::Compact));
    } else {
        sender->sendTextMessage(QJsonDocument(createStatusMessage(QStringLiteral("error"), QStringLiteral("Nieznana komenda."))).toJson(QJsonDocument::Compact));}}


void RemoteServer::captureScreen() {
    if (m_screenProcess->state() != QProcess::NotRunning) return;
    m_screenProcess->readAllStandardOutput(); 
    QStringList args;
    QString targetSerial = m_executor->targetDevice(); 
    QString adbPath = m_executor->adbPath();
    if (!targetSerial.isEmpty()) {
        args << "-s" << targetSerial;}
    // wysyła surowy bufor klatek (Raw Frame Buffer)
    args << "exec-out" << "screencap"; 
    m_screenProcess->start(adbPath, args);}

// Raw Frame Buffer (13 x 4 bajty = 52 bajty)
struct FramebufferHeader {
    quint32 width;
    quint32 height;
    quint32 bpp;
    quint32 size;
    quint32 offset; // Zawsze 0 w nowszych wersjach
    quint32 red_offset;
    quint32 red_length;
    quint32 green_offset;
    quint32 green_length;
    quint32 blue_offset;
    quint32 blue_length;
    quint32 alpha_offset;
    quint32 alpha_length;
};


void RemoteServer::onScreenCaptured(int exitCode, QProcess::ExitStatus) {
    const int HEADER_SIZE = 52; 
    if (exitCode == 0 && !m_clients.isEmpty()) {
        QByteArray data = m_screenProcess->readAllStandardOutput();
        if (data.size() < HEADER_SIZE) {
            qWarning() << "Screen capture failed: Data too short for raw frame buffer.";
            return;}
        const FramebufferHeader *header = reinterpret_cast<const FramebufferHeader*>(data.constData());
        quint32 w = header->width;
        quint32 h = header->height;
        quint32 bpp = header->bpp;
        if (bpp != 32) {
            qWarning() << "Unsupported BPP:" << bpp << ". Expected 32-bit format (RGBA/BGRA). Skipping frame.";
            return;}        
        const char* pixelData = data.constData() + HEADER_SIZE;
        int pixelDataSize = data.size() - HEADER_SIZE;
        if (pixelDataSize < (int)w * (int)h * (bpp / 8)) {
            qWarning() << "Incomplete pixel data received.";
            return;}
        QImage screenImage(
            (const uchar*)pixelData, 
            (int)w, 
            (int)h, 
            QImage::Format_RGB32 
        );
        if (!screenImage.isNull()) {
            QByteArray imageBytes;
            QBuffer buffer(&imageBytes);
            buffer.open(QIODevice::WriteOnly);
            screenImage.save(&buffer, "JPG", 70); 
            QJsonObject json;
            json["type"] = "screen";
            json["data"] = QString(imageBytes.toBase64());
            sendMessageToAll(QJsonDocument(json).toJson(QJsonDocument::Compact));
        } else {
             qWarning() << "Failed to create QImage from raw data.";}
    } else if (exitCode != 0) {
        QString error = m_screenProcess->readAllStandardError();
        if (!error.isEmpty()) {
            qWarning() << "Screen capture failed:" << error;}}}


void RemoteServer::onRunnerLog(const QString &text, const QString &color) {
    Q_UNUSED(color);
    sendMessageToAll(QJsonDocument(createLogMessage(text)).toJson(QJsonDocument::Compact));}


void RemoteServer::onRunnerFinished(bool success) {
    sendMessageToAll(QJsonDocument(createLogMessage(success ? QStringLiteral("Sekwencja zakończona sukcesem.") : QStringLiteral("Sekwencja zakończona błędem."), 
                                   success ? QStringLiteral("success") : QStringLiteral("error"))).toJson(QJsonDocument::Compact));
    sendMessageToAll(QJsonDocument(createStatusMessage(QStringLiteral("finished"), success ? QStringLiteral("Success") : QStringLiteral("Failure"))).toJson(QJsonDocument::Compact));}

void RemoteServer::sendMessageToAll(const QString &message) {
    for (QWebSocket *client : m_clients) {
        if (client->isValid()) {
            client->sendTextMessage(message);}}}

QJsonObject RemoteServer::createLogMessage(const QString &text, const QString &type) const {
    QJsonObject json;
    json[QStringLiteral("type")] = QStringLiteral("log");
    json[QStringLiteral("timestamp")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    json[QStringLiteral("message")] = text;
    json[QStringLiteral("logType")] = type;
    return json;}

QJsonObject RemoteServer::createStatusMessage(const QString &status, const QString &message) const {
    QJsonObject json;
    json[QStringLiteral("type")] = QStringLiteral("status");
    json[QStringLiteral("status")] = status;
    json[QStringLiteral("message")] = message;
    return json;}

```


```
//remoteserver.h
#pragma once

#include <QObject>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QList>
#include <QJsonObject>
#include <QHostAddress>
#include <QTimer>
#include <QProcess>

class CommandExecutor;
class SequenceRunner;

class RemoteServer : public QObject {
    Q_OBJECT
public:
    explicit RemoteServer(const QString &adbPath, const QString &targetSerial, 
                          quint16 port = 12345, QObject *parent = nullptr);
    ~RemoteServer();

private slots:
    void onNewConnection();
    void onSocketDisconnected();
    void onTextMessageReceived(const QString &message);
    void onRunnerLog(const QString &text, const QString &color);
    void onRunnerFinished(bool success);
    
    void captureScreen();
    void onScreenCaptured(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QWebSocketServer *m_wsServer = nullptr;
    QList<QWebSocket *> m_clients;
    CommandExecutor *m_executor = nullptr;
    SequenceRunner *m_runner = nullptr;
    
    QTimer *m_screenTimer = nullptr;
    QProcess *m_screenProcess = nullptr;

    void sendMessageToAll(const QString &message);
    void handleCommand(QWebSocket *sender, const QJsonObject &json);
    QJsonObject createLogMessage(const QString &text, const QString &type = QStringLiteral("info")) const;
    QJsonObject createStatusMessage(const QString &status, const QString &message) const;
};
```

```
//sequencerunner.cpp
#include "sequencerunner.h"
#include "commandexecutor.h"
#include "argsparser.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

SequenceRunner::SequenceRunner(CommandExecutor *executor, QObject *parent)
    : QObject(parent), m_executor(executor) {
    m_delayTimer.setSingleShot(true);
    connect(&m_delayTimer, &QTimer::timeout, this, &SequenceRunner::onDelayTimeout);
    connect(m_executor, &CommandExecutor::finished, this, &SequenceRunner::onCommandFinished);
}

SequenceRunner::~SequenceRunner() {}

SequenceCmd SequenceRunner::parseCommandFromJson(const QJsonObject &obj) {
    SequenceCmd cmd;
    cmd.command = obj.value("command").toString();
    // Zmieniono domyślne opóźnienie z 1000ms na 100ms
    cmd.delayAfterMs = obj.value("delayAfterMs").toInt(100);
    cmd.runMode = obj.value("runMode").toString("adb").toLower(); 
    cmd.stopOnError = obj.value("stopOnError").toBool(true);
    cmd.successCommand = obj.value("successCommand").toString();
    cmd.failureCommand = obj.value("failureCommand").toString();
    return cmd;}

void SequenceRunner::clearSequence() {
    m_commands.clear();
    emit logMessage("Sequence queue cleared.", "#BDBDBD");}

bool SequenceRunner::appendSequence(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit logMessage(QString("Cannot open file: %1").arg(file.errorString()), "#F44336");
        return false;}
    QByteArray jsonData = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) {
        emit logMessage("File does not contain a valid JSON array.", "#F44336");
        return false;}
    return loadSequenceFromJsonArray(doc.array());}

bool SequenceRunner::loadSequenceFromJsonArray(const QJsonArray &array) {
    if (m_isRunning) {
        emit logMessage("Cannot load sequence while one is running.", "#F44336");
        return false;}
    m_commands.clear();
    for (const QJsonValue &value : array) {
        if (value.isObject()) {
            m_commands.append(parseCommandFromJson(value.toObject()));
        } else {
            emit logMessage("Invalid command format in JSON array.", "#F44336");
            return false;}}
    emit logMessage(QString("Loaded %1 commands.").arg(m_commands.count()), "#4CAF50");
    return true;}

QStringList SequenceRunner::getCommandsAsText() const {
    QStringList list;
    for (const auto &cmd : m_commands) {
        QString text = cmd.command;
        if (!cmd.successCommand.isEmpty()) {
            text += QString(" (Sukces: '%1')").arg(cmd.successCommand);}
        if (!cmd.failureCommand.isEmpty()) {
            text += QString(" (Błąd: '%1')").arg(cmd.failureCommand);}
        list.append(text);}
    return list;}

bool SequenceRunner::startSequence() {
    if (m_commands.isEmpty()) {
        emit logMessage("Sequence is empty. Nothing to start.", "#FFC107");
        return false;}
    if (m_isRunning) {
        emit logMessage("Sequence is already running.", "#FFC107");
        return true;}
    m_currentIndex = 0;
    m_isRunning = true;
    emit sequenceStarted();
    emit logMessage("--- SEQUENCE STARTED ---", "#009688");
    executeNextCommand();
    return true;}

void SequenceRunner::stopSequence() {
    if (!m_isRunning) return;
    m_executor->cancelCurrentCommand();
    m_delayTimer.stop();
    m_isRunning = false;
    finishSequence(false);}

void SequenceRunner::executeNextCommand() {
    if (!m_isRunning || m_currentIndex >= m_commands.count()) {
        finishSequence(true);
        return;}
    const SequenceCmd &cmd = m_commands.at(m_currentIndex);
    if (cmd.isConditionalExecution) {
        emit logMessage(QString("Wykonywanie komendy warunkowej: %1").arg(cmd.command), "#FF9800");
    } else {
        emit commandExecuting(cmd.command, m_currentIndex + 1, m_commands.count());}
    if (cmd.runMode == "root") {
        m_executor->executeRootShellCommand(cmd.command);
    } else if (cmd.runMode == "shell") {
        m_executor->executeShellCommand(cmd.command);
    } else {
        m_executor->executeAdbCommand(cmd.command);}}

void SequenceRunner::onDelayTimeout() {
    executeNextCommand();}

void SequenceRunner::executeConditionalCommand(const QString& cmd, const QString& runMode, bool isSuccess) {
    if (cmd.isEmpty()) return;
    SequenceCmd conditionalCmd;
    conditionalCmd.command = cmd;
    conditionalCmd.runMode = runMode;
    conditionalCmd.delayAfterMs = 0;
    conditionalCmd.stopOnError = true;
    conditionalCmd.isConditionalExecution = true;
    m_commands.insert(m_currentIndex + 1, conditionalCmd);
    emit logMessage(QString("Wstrzyknięto komendę warunkową (ExitCode: %1): '%2'").arg(isSuccess ? "0 (Sukces)" : "!=0 (Błąd)", cmd), "#2196F3");}

void SequenceRunner::onCommandFinished(int exitCode, QProcess::ExitStatus) {
    if (!m_isRunning) return;
    const SequenceCmd &currentCmd = m_commands.at(m_currentIndex);
    if (!currentCmd.isConditionalExecution) {
        if (exitCode == 0) {
            executeConditionalCommand(currentCmd.successCommand, currentCmd.runMode, true);
        } else {
            executeConditionalCommand(currentCmd.failureCommand, currentCmd.runMode, false);}}
    if (exitCode != 0) {
        if (currentCmd.stopOnError) {
            emit logMessage(QString("Sekwencja zatrzymana: Komenda nie powiodła się (kod %1).").arg(exitCode), "#F44336");
            finishSequence(false);
            return;}}
    m_currentIndex++;
    if (currentCmd.isConditionalExecution) {
        m_commands.removeAt(m_currentIndex - 1);
        m_currentIndex--;
    }
    if (m_currentIndex < m_commands.count()) {
        if (currentCmd.delayAfterMs > 0) {
            emit logMessage(QString("Oczekiwanie %1 ms...").arg(currentCmd.delayAfterMs), "#FFC107");
            m_delayTimer.setInterval(currentCmd.delayAfterMs);
            m_delayTimer.start();
        } else {
            executeNextCommand();}
    } else {
        finishSequence(true);}}

void SequenceRunner::finishSequence(bool success) {
    if (!m_isRunning) return;
    m_isRunning = false;
    m_delayTimer.stop();
    m_executor->cancelCurrentCommand(); 
    emit sequenceFinished(success);
    if (m_isInterval) {
        if (success) {
            emit logMessage(QString("Sekwencja zakończona sukcesem. Restart za %1 sekund.").arg(m_intervalValueS), "#00BCD4");
        } else {
            emit logMessage(QString("Sekwencja zakończona błędem. Restart za %1 sekund.").arg(m_intervalValueS), "#00BCD4");}
        emit scheduleRestart(m_intervalValueS);
    } else {
        if (success) {
            emit logMessage("--- SEKWENCJA ZAKOŃCZONA SUKCESEM ---", "#4CAF50");
        } else {
            emit logMessage("--- SEKWENCJA ZAKOŃCZONA BŁĘDEM ---", "#F44336");}}}

void SequenceRunner::setIntervalToggle(bool toggle) {
    m_isInterval = toggle;
    emit logMessage(QString("Interwał sekwencji: %1").arg(toggle ? "WŁĄCZONY" : "WYŁĄCZONY"), "#BDBDBD");}

void SequenceRunner::setIntervalValue(int seconds) {
    m_intervalValueS = seconds;
    emit logMessage(QString("Wartość interwału ustawiona na %1 s.").arg(seconds), "#BDBDBD");}

```


```
//sequencerunner.h
#ifndef SEQUENCERUNNER_H
#define SEQUENCERUNNER_H

#include <QProcess>
#include <QObject>
#include <QList>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>

class CommandExecutor;

struct SequenceCmd {
    QString command;
    int delayAfterMs = 0;
    QString runMode = "adb"; // "adb", "shell", "root"
    bool stopOnError = true;

    QString successCommand;
    QString failureCommand;
    bool isConditionalExecution = false;
};

class SequenceRunner : public QObject {
    Q_OBJECT
public:
    explicit SequenceRunner(CommandExecutor *executor, QObject *parent = nullptr);
    ~SequenceRunner() override;
    bool appendSequence(const QString &filePath);
    void clearSequence();
    bool startSequence();
    void stopSequence();
    void setIntervalToggle(bool toggle);
    void setIntervalValue(int seconds);
    bool isRunning() const { return m_isRunning; }
    QStringList getCommandsAsText() const;
    int commandCount() const { return m_commands.count(); }
    bool loadSequenceFromJsonArray(const QJsonArray &array);

signals:
    void sequenceStarted();
    void sequenceFinished(bool success);
    void scheduleRestart(int intervalSeconds);
    void commandExecuting(const QString &cmd, int index, int total);
    void logMessage(const QString &text, const QString &color);

private slots:
    void onCommandFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onDelayTimeout();

private:
    CommandExecutor *m_executor;
    QList<SequenceCmd> m_commands;
    QTimer m_delayTimer;
    int m_currentIndex = 0;
    bool m_isRunning = false;
    bool m_isInterval = false;
    int m_intervalValueS = 60;
    void finishSequence(bool success);
    void executeNextCommand();
    SequenceCmd parseCommandFromJson(const QJsonObject &obj);
    void executeConditionalCommand(const QString& cmd, const QString& runMode, bool isSuccess);
};

#endif // SEQUENCERUNNER_H

```












