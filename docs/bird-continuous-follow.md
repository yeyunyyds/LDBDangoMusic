# Bird continuous follow (development test)

- Small actor displacement (24 units or 15 degrees) extends a collision-checked waypoint at 100 ms cadence. Reframe cooldown is separate from tracking. Slow follow uses 90–330 units/s cap; smooth relocation 180, fast 650.
- Occlusion grace 1500 ms, recovery search including nearby candidates; safe cut eligible after 5000 ms or severe distance loss. Co-op combat view can remain valid with visible teammate and threat.
- Fast relocation selection probability halves for each consecutive fast relocation: 1, 1/2, 1/4, ...; safe cut or slow move resets streak. Sustained continuous tracking also resets after 1.5s. Draw only on long relocation decisions. No unsafe cut.
- Tank engagement weighting, actor-aware framing, heights 480/320 replacing 540/350.
- Bounded short segment tests rather than navmesh routing. Runtime tuning remains experimental. No public release.
