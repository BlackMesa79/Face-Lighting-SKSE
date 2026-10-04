# SE / AE compatibility checks

Local gameplay testing is primarily on AE 1.6.1170. It must not be treated as validation of SE 1.5.97.

For changes touching engine objects, prefer the vendored CommonLib runtime accessors over inherited base casts or direct runtime-dependent members. Review virtual dispatch separately. Test native layouts for SE 1.5.97 and AE 1.6.1170, including version boundaries when relevant. These fixtures do not replace gameplay testing on both runtimes.

## Actor life-state correction

The released 0.9.2 used `actor->GetLifeState()`. Its compiler-generated ActorState upcast used 0xA0, reading flags at actor+0xA8. Native ActorState lives at 0xB8 before 1.6.629 and 0xC0 from 1.6.629 onward. Shared validation could consequently reject living actors in any lighting source.

The 0.9.3 fix uses `actor->AsActorState()->GetLifeState()` and retains the existing death, deletion and visibility safeguards. ActorRuntimeTests now mock 1.5.97, 1.6.353, 1.6.629 and 1.6.1170, writing flags directly at independent native byte offsets. They verify living actors with poisoned old/other-version offsets, dying/dead/recycled rejection, and the SE/AE IsDead vtable dispatch. The testing macro is enabled only for this test target.

Thanks to [jinx60](https://next.nexusmods.com/profile/jinx60) for discovering and helping diagnose this issue. The fix is packaged separately as 0.9.3; the existing 0.9.2 release archive is not replaced. User-side 1.5.97 testing of the source-built fix, especially Ashe recruitment and manual selection through hotkey/console/menu, remains necessary.
