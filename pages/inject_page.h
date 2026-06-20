#ifndef INJECT_PAGE_H
#define INJECT_PAGE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QLineEdit>
#include <QThread>

class FH6Injector;
class ZoomableGraphicsView;

class InjectPage : public QWidget {
    Q_OBJECT
public:
    explicit InjectPage(QWidget* parent = nullptr);
    ~InjectPage();

private slots:
    void refreshProcesses();
    void browseJsonFile();
    void startInjection();
    void stopInjection();
    void appendLog(const QString& msg);
    void updateProgress(int current, int total);
    void injectionFinished(bool success);

private:
    QComboBox* processCombo;
    QPushButton* refreshProcessBtn;
    
    QLineEdit* jsonPathEdit;
    QPushButton* browseJsonBtn;
    
    QSpinBox* layerCountSpin;

    ZoomableGraphicsView* previewLabel;
    QLabel* shapeInfoLabel;
    
    QPushButton* injectBtn;
    QPushButton* stopBtn;
    
    QProgressBar* progressBar;
    QTextEdit* logArea;

    QThread* injectorThread;
    FH6Injector* injector;
};

#endif // INJECT_PAGE_H
