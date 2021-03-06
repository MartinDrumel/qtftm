#ifndef AMDORPLOT_H
#define AMDORPLOT_H

#include "zoompanplot.h"

#include <qwt6/qwt_symbol.h>

class QwtPlotCurve;

class AmdorPlot : public ZoomPanPlot
{
    Q_OBJECT
public:
    AmdorPlot(QWidget *parent = nullptr);

    void setPlotRange(double min, double max);

    // QObject interface
    bool eventFilter(QObject *obj, QEvent *ev);

public slots:
    void updateData(QList<QVector<QPointF> > l);
    void toggleCurveVisibility(QVariant item, bool hide, int index);
    void changeCurveColor(QwtPlotCurve *c, QColor currentColor);
    void changeCurveSymbol(QwtPlotCurve *c, QwtSymbol::Style sty);
    void changeCurveSize(QwtPlotCurve *c, int size);

    // ZoomPanPlot interface
protected:
    void filterData();

private:
    QList<QwtPlotCurve*> d_curves;
    QwtPlotCurve *p_diagCurve;

    QMenu *curveContextMenu(QwtPlotCurve *c);

};

#endif // AMDORPLOT_H
