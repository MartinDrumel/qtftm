#ifndef BATCHVIEWWIDGET_H
#define BATCHVIEWWIDGET_H

#include <QWidget>
#include "batchmanager.h"
#include "abstractbatchplot.h"
#include <QThread>

namespace Ui {
class BatchViewWidget;
}

class BatchViewWidget : public QWidget
{
    Q_OBJECT
    
public:
    explicit BatchViewWidget(BatchManager::BatchType type, int num, double delay, int hpf, double exp, bool rDC = true, bool pad = false, QWidget *parent = nullptr);
    ~BatchViewWidget();

public slots:
    void process();
    void processingComplete(bool failure);
    
private:
    Ui::BatchViewWidget *ui;
    AbstractBatchPlot *batchPlot;

    QThread *batchThread;
    int d_number;
    BatchManager::BatchType d_type;
    int d_firstScan;
    int d_lastScan;

signals:
    void metaDataChanged(int);
    void checkForMetaDataChanged(int);

};

#endif // BATCHVIEWWIDGET_H