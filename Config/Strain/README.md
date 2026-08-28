# Strain limits

`AugmentedHumanoid.json` is the **body** budget for a future-war Guardian-class frame.

`maxFallBeforeCriticalCm` (30 m) is how far you can fall before a landing is critical. Recast AirDiveDown / AirDiveOver depth, Launch search XY, and a later fall-damage hook all read this number. Do not copy `3000` into nav or greybox.

Greybox rescue Z stays below the island (survivable fall minus end-tol below apex) so a max-envelope dive is not yanked mid-fall. Standing on the island recalls to the lip.

This pass stores the number and logs it on land. It does not apply HP yet.
