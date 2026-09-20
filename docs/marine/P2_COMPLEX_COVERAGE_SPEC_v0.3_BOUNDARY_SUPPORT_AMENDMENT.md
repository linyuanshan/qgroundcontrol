# P2 Complex Coverage Specification — Boundary Support Amendment

Status: P2-09 design-authority amendment

This amendment supersedes only the P2 v0.2 statement that a failed nominal completeness check must not add a
boundary pass. All other P2 v0.2 architecture, safety, geometry, and acceptance requirements remain in force.

## Reason for the amendment

P2-09 produced a deterministic counterexample with a 20 m by 20 m outer boundary, a 4 m square No-Go region,
1 m safety margin, 4 m swath, and a 90 degree sweep. The valid shared-lattice cell coverage leaves nominal target
area uncovered near the physical safety bands.

For swath width `w`, footprint radius `r = w / 2`, safety margin `s`, and lane spacing `d`, round-cap coverage near
a sweep endpoint requires:

```text
sqrt(s^2 + (d / 2)^2) <= r
```

With `w = 4`, `r = 2`, and `s = 1`, a nominal 4 m lane spacing cannot satisfy this bound. When `s = r`, no finite
positive lane spacing can satisfy it. Changing cell lane phase therefore cannot provide the general closure while
preserving the frozen swath and safety semantics.

## Frozen coverage responsibilities

```text
CellCoverage
    = deterministic interior coverage on the shared global lane schedule

BoundaryCoverageSupport
    = Coverage-role traversal of every TrackFeasibleRegion outer and hole boundary
      for nominal safety-band support

NominalCoverageValidator
    = final geometric truth for CoverageTarget completeness
```

`CoverageTarget` remains `WorkRegion - NoGo`. `TrackFeasibleRegion` remains the outer-boundary inset minus inflated
No-Go regions. They are not interchangeable.

Boundary support uses the already validated backend-derived TrackFeasibleRegion vertices without offsetting,
simplification, snapping, smoothing, or BCD changes. Every component is an explicitly closed canonical polyline;
its legs have `PathLegRole::Coverage`. Safe routes between boundary components and from the last component to the
first ordered cell have `PathLegRole::Transit`.

The deterministic assembly order is:

```text
boundary components in stable canonical ID order
    -> GreedyCellOrdering result
```

Coverage length includes cell Coverage legs and boundary-support legs. Transit length includes cell connectors,
boundary-component routes, the boundary-to-first-cell route, and inter-cell routes. The canonical path plus
`PathLegRole[]` remains the sole assembled-path truth.

Boundary support does not assert completeness by itself. The unchanged P2-09 validator buffers only Coverage-role
legs and compares the resulting footprint with CoverageTarget using the frozen area tolerance. A failed validation
remains `CoverageIncomplete`; no additional repair path is generated.
