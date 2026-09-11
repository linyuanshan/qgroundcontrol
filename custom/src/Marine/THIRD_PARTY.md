# Marine Third-Party Dependencies

## Clipper2 2.0.1

- Source: <https://github.com/AngusJohnson/Clipper2>
- Pinned tag: `Clipper2_2.0.1`
- License: Boost Software License 1.0
- Integration: fetched by CPM in download-only mode and compiled as the Marine-private `MarineClipper2` static target.
- Use in P1: polygon offsetting behind the Marine-owned geometry API. No Clipper2 type is exposed outside the implementation.

The upstream license text is available at <https://github.com/AngusJohnson/Clipper2/blob/Clipper2_2.0.1/LICENSE>.

## Fork-Level Dependency Policy

This QGroundControl fork pins Clipper2 to `Clipper2_2.0.1`, records its license and source here, and limits the
dependency to the Marine custom build. This attribution record does not amend upstream QGroundControl licensing
policy and does not make a legal compatibility determination. Any distribution decision remains subject to the
fork maintainers' normal legal and project-governance review.
