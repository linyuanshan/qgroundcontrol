#pragma once

#include "UnitTest.h"

class MarineGeometryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _testRectangleInset();
    void _testZeroMargin();
    void _testEmptyInset();
    void _testDisconnectedInset();
    void _testOrientationIndependence();
    void _testInvalidInput();
    void _testDeterminismAndPrecision();
    void _testSweepFrameTransform();
    void _testPolygonMonotonicity();
    void _testScanlineIntervals();
    void _testScanlineVertexAndBoundaryCases();
};
