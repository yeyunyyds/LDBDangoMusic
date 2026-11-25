# Bird continuous follow (development test)

- Small actor displacement (24 units or 15 degrees) extends a collision-checked waypoint at 100 ms cadence. Reframe cooldown is separate from tracking. Normal tracking and ordinary reframing use 50 units/s, acceleration 90 units/s². Actor speed alone never raises the cap. Catch-up becomes eligible only when relative displacement exceeds 180 units and the camera distance is outside the relative composition envelope or the actor-only view angle changes over 35°. Initial positioning and significant recovery/alternative relocation may use 180; fast catch-up 650. Velocity ramps across caps; collision safety still overrides smoothing.
- Occlusion grace 1500 ms, recovery search including nearby candidates; safe cut eligible after 5000 ms or severe distance loss. Co-op combat view can remain valid with visible teammate and threat.
- Fast relocation selection probability halves for each consecutive fast relocation: 1, 1/2, 1/4, ...; safe cut or slow move resets streak. Sustained continuous tracking also resets after 1.5s. Draw only on long relocation decisions when catch-up is actually needed. No unsafe cut.
- Tank engagement weighting, actor-aware framing, heights 480/320 replacing 540/350.
- Bounded short segment tests rather than navmesh routing. Runtime tuning remains experimental. No public release.

Recovery onset ignores ordinary reframe cooldown; active collision-clear relocations are not replaced by ordinary shot timers. During the initial occlusion grace only bounded clear tracking segments may continue without subject sight. Recovery tries nearby routes before remote repositioning.
