# Strain limits

`AugmentedHumanoid.json` is the **body** budget for a future-war Guardian-class frame.

`maxFallBeforeCriticalCm` (30 m) is how far you can fall before a landing is critical. Recast AirDive `JumpMaxDepth` uses **Abs(triple apex − 3000) ≈ 1420** (NavTune token `apexFallAbs`). Do not paste `3000` as JumpMaxDepth. A later fall-damage hook still reads the raw 30 m.

Greybox rescue Z stays below the island (survivable fall minus end-tol below apex) so a max-envelope dive is not yanked mid-fall. Standing on the island recalls to the lip.

This pass stores the number and logs it on land. It does not apply HP yet.
