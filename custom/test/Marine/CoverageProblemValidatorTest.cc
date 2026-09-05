#include "CoverageProblemValidatorTest.h"

#include <limits>

#include "CoverageProblemValidator.h"

using namespace Marine;

namespace {

CoveragePlanningProblem validProblem()
{
    CoveragePlanningProblem problem;
    problem.region.outerBoundary.vertices = {
        {0.0, 0.0},
        {20.0, 0.0},
        {20.0, 10.0},
        {0.0, 10.0},
    };
    problem.swathWidthM = 6.0;
    problem.safetyMarginM = 2.0;
    problem.sweepAngleMode = SweepAngleMode::Manual;
    problem.requestedSweepAngleDeg = 45.0;
    return problem;
}

void verifyInvalidOuterBoundary(CoveragePlanningProblem problem)
{
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidOuterBoundary);
}

}  // namespace

void CoverageProblemValidatorTest::_testPolygonValidation()
{
    CoveragePlanningProblem problem = validProblem();
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);

    problem = validProblem();
    problem.region.outerBoundary.vertices = {{0.0, 0.0}, {10.0, 0.0}};
    verifyInvalidOuterBoundary(problem);

    problem = validProblem();
    problem.region.outerBoundary.vertices = {{0.0, 0.0}, {10.0, 0.0}, {10.0, 0.0}, {0.0, 10.0}};
    verifyInvalidOuterBoundary(problem);

    problem = validProblem();
    problem.region.outerBoundary.vertices[1] = {0.0005, 0.0};
    verifyInvalidOuterBoundary(problem);

    problem = validProblem();
    problem.region.outerBoundary.vertices[1] = {0.002, 0.0};
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);

    problem = validProblem();
    problem.region.outerBoundary.vertices = {{0.0, 0.0}, {10.0, 0.0}, {20.0, 0.0}};
    verifyInvalidOuterBoundary(problem);

    problem = validProblem();
    problem.region.outerBoundary.vertices = {{0.0, 0.0}, {10.0, 10.0}, {0.0, 10.0}, {10.0, 0.0}};
    verifyInvalidOuterBoundary(problem);

    problem = validProblem();
    problem.region.outerBoundary.vertices.front().xM = std::numeric_limits<double>::quiet_NaN();
    verifyInvalidOuterBoundary(problem);
}

void CoverageProblemValidatorTest::_testNumericValidation()
{
    CoveragePlanningProblem problem = validProblem();
    problem.swathWidthM = 0.0;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSwathWidth);

    problem = validProblem();
    problem.swathWidthM = std::numeric_limits<double>::infinity();
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSwathWidth);

    problem = validProblem();
    problem.safetyMarginM = -0.01;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSafetyMargin);

    problem = validProblem();
    problem.safetyMarginM = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSafetyMargin);

    problem = validProblem();
    problem.safetyMarginM = 3.01;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSafetyMargin);

    problem = validProblem();
    problem.safetyMarginM = problem.swathWidthM / 2.0;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);
}

void CoverageProblemValidatorTest::_testAngleNormalization()
{
    CoveragePlanningProblem problem = validProblem();
    problem.requestedSweepAngleDeg = 225.0;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);
    QCOMPARE(problem.requestedSweepAngleDeg, 45.0);

    problem = validProblem();
    problem.requestedSweepAngleDeg = -45.0;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);
    QCOMPARE(problem.requestedSweepAngleDeg, 135.0);

    problem = validProblem();
    problem.requestedSweepAngleDeg = 540.0;
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);
    QCOMPARE(problem.requestedSweepAngleDeg, 0.0);

    problem = validProblem();
    problem.requestedSweepAngleDeg = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSweepAngle);

    problem = validProblem();
    problem.sweepAngleMode = SweepAngleMode::Auto;
    problem.requestedSweepAngleDeg = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::None);
    QCOMPARE(problem.requestedSweepAngleDeg, 0.0);

    problem = validProblem();
    problem.sweepAngleMode = static_cast<SweepAngleMode>(99);
    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::InvalidSweepAngle);
}

void CoverageProblemValidatorTest::_testNoGoCapabilityGate()
{
    CoveragePlanningProblem problem = validProblem();
    problem.region.noGoRegions.push_back({{{2.0, 2.0}, {3.0, 2.0}, {2.0, 3.0}}});

    QCOMPARE(CoverageProblemValidator::validateAndNormalize(problem), CoveragePlanningError::UnsupportedNoGoRegion);
}

void CoverageProblemValidatorTest::_testErrorMapping()
{
    QCOMPARE(CoverageProblemValidator::statusForError(CoveragePlanningError::None), PlanningStatus::Success);
    QCOMPARE(CoverageProblemValidator::statusForError(CoveragePlanningError::InvalidOuterBoundary),
             PlanningStatus::InvalidInput);
    QCOMPARE(CoverageProblemValidator::statusForError(CoveragePlanningError::InvalidSwathWidth),
             PlanningStatus::InvalidInput);
    QCOMPARE(CoverageProblemValidator::statusForError(CoveragePlanningError::InvalidSafetyMargin),
             PlanningStatus::InvalidInput);
    QCOMPARE(CoverageProblemValidator::statusForError(CoveragePlanningError::InvalidSweepAngle),
             PlanningStatus::InvalidInput);
    QCOMPARE(CoverageProblemValidator::statusForError(CoveragePlanningError::UnsupportedNoGoRegion),
             PlanningStatus::Failed);

    QVERIFY(!CoverageProblemValidator::messageForError(CoveragePlanningError::InvalidOuterBoundary).empty());
    QVERIFY(CoverageProblemValidator::messageForError(CoveragePlanningError::UnsupportedNoGoRegion).find("P2") !=
            std::string::npos);
    QVERIFY(CoverageProblemValidator::messageForError(CoveragePlanningError::InvalidSafetyMargin).find("swath") !=
            std::string::npos);
}

UT_REGISTER_TEST_LIGHTWEIGHT(CoverageProblemValidatorTest, TestLabel::Unit)
