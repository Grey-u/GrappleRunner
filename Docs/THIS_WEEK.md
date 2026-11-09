# Grapple Runner: your week, 10 to 15 Nov 2026 (doable without Claude)

Use this when Claude's weekly usage runs out. One task a day, about 1 to 2 hours each.
Each task ends with a **commit**, so your portfolio grows every day and the commits are yours.

> **Golden rules this week**
> - **Save every 10 minutes:** `Ctrl+S` saves the level; File, then Save All, saves everything. Today's blackout is why.
> - **Commit at the end of every session** (GitHub Desktop, steps at the bottom).
> - **One task per day.** If you want to polish something else, write it in `NOW.md` under "Next" instead. That is your own perfectionism rule.

---

## Monday 9 Nov, tonight (10 min): check nothing broke in the blackout

The good news: **the level was saved at 15:35**, after the plants and music went in. All the code is on your PC and on GitHub. The one risk is the two packs that were still installing when the power went: **Paragon Agora & Monolith** and **European Beech** (it installs as the `KiteDemo` folder).

1. Open the **Epic Games Launcher**, then **Fab**, then **My Library**.
2. Search `Agora`, click **+**, tick **Show all projects** if needed, click **GrappleRunner**, then **Add to Project**.
   - You should see a "Files downloaded" pop-up. Re-adding repairs a half-finished install; it won't make a duplicate.
3. Do the same for `European Beech`. If it asks for a version, pick **5.6**.
4. Open the project: **Unreal Engine** tab, then **Library**, then double-click **GrappleRunner**.
   - If it asks *"rebuild modules?"*, click **Yes** and wait about a minute.
   - You should see the floating islands with grass.
5. Turn on autosave: **Edit**, then **Editor Preferences**. Search `auto save`, tick **Enable AutoSave**, and set **Frequency in Minutes** to **5**.

If Unreal shows a crash or "failed to load" message, take a photo of it and save it for Claude.

---

## Tuesday 10 Nov: build ruins on the islands (cover and tactical retreats)

**Goal:** three to five ruin "clusters" on the landing islands that you could hide behind or retreat through.

1. Open the project. At the bottom-left, click **Content Drawer**.
   - A file browser for the project slides up.
2. Open the folder `InfinityBladeGrassLands`, then `Environments`, then `Plains`, then `Env_Plains_Ruins` (also try `Env_Plains_Statues`).
   - You should see pillars, broken walls, arches and stairs.
3. **Drag** a broken wall or pillar into the level, onto an island near the start.
4. Make it sit on the ground: with it selected, press **End**.
   - It drops onto the surface below.
5. Move, rotate and scale it with the coloured arrows:
   - **W** = move, **E** = rotate, **R** = scale.
   - Hold **Alt** while dragging an arrow to make a copy.
6. Build a cluster: two or three walls, a pillar, an arch. Leave gaps so a player can weave through. Do the same with `ParagonProps`, then `Monolith` and `Agora`, for one big ancient structure per two or three islands.
7. **Test it:** click **Play** (green ▶) and walk or swing around it. Press **Esc** to stop.
   - Hooks should catch on the ruins, because the ruins are solid.
8. **Save** (`Ctrl+S`) and **commit** with the message: `Add ruin clusters to the floating islands`.

**Portfolio tip:** take a screenshot before and after (`Win+Shift+S`) and keep them in `Docs/media/`.

---

## Wednesday 11 Nov: trees, jungle feel and water, measured

Make the level feel like a jungle, and *measure* what it costs. That turns it into a portfolio test.

**A. Measure first (5 min)**
1. Click **Play**, then click inside the game. Press the **`** key (the key under Esc) to open the console.
2. Type `gr.Test perf` and press **Enter**. Wait 10 seconds.
   - A cyan line appears with the frame-time results.
3. Press **Esc** to stop. Write the numbers down: median ms, 95th, 99th, and % over 11.1 ms.

**B. Paint trees**
1. Top-left, change **Selection Mode** to **Foliage** (or press `Shift+3`).
2. Drag a tree mesh from `KiteDemo` (European Beech) into the foliage panel's list.
   - The folder is `KiteDemo`, then `Environments`, then `Trees`. Ferns and bushes are in `Environments`, then `Foliage`.
3. Set **Density** low (about 5) and paint on the island tops by left-click dragging. **Shift** + drag erases.
4. Keep trees **away from the swing route's anchor rocks**, so hooks still have clear lines.

**C. Water**
1. Waterfalls already exist (folder `GrappleRunner/Waterfalls` in the Outliner, top-right). Select one and **Alt+drag** to copy it to more islands.
2. For a pool: in Place Actors (left), search `Water Body Lake`. Drag one onto a flat island top and shrink it.
   - If it's not there, skip it; Claude can add water next week.

**D. Measure again** by repeating step A. Then copy this into `Docs/TESTS.md` as **T-15** and fill in the blanks:

```
## T-15: Frame-time cost of trees and water
**Question.** How much frame time do the trees and water cost?
**Method.** gr.Test perf before and after, same view, same session.
**Results.** Before: median __ ms, over 11.1 ms __%.  After: median __ ms, over 11.1 ms __%.
**Conclusion.** The scenery costs __ ms per frame. (If more than ~1 ms: fewer trees, or ask Claude for LODs and cull distances.)
```

Commit with the message: `Jungle dressing: trees and water, measured in T-15`.

---

## Thursday 12 Nov: your avatar in Blender (Kwang)

**Goal:** a version of Kwang you've edited, still rigged, to become the first-person body. Unreal stays unchanged today, so nothing can break.

1. In Unreal's Content Drawer, open `ParagonKwang`, then `Characters`, then `Heroes`, then `Kwang`, then `Meshes`.
2. Right-click the **skeletal mesh** (pink bar under the thumbnail), then **Asset Actions**, then **Export**. Save as FBX into `GrappleRunner/Art/Characters/`.
3. In **Blender**: File, then Import, then FBX. Tick **Automatic Bone Orientation**, then click Import.
   - You should see Kwang with an armature (bones).
4. Edit carefully:
   - **Don't delete or rename bones.**
   - Change shapes in **Edit Mode** or **Sculpt Mode**, and colours in the material.
   - Rotate a bone in **Pose Mode** to check the mesh still follows it.
5. Export: File, then Export, then FBX. Tick **Selected Objects** with the mesh *and* armature selected, set **Add Leaf Bones** OFF, and save as `SK_Naomi_Kwang.fbx`.
6. **Don't import it into Unreal yet.** Claude will do that with the first-person body code.

**Licence note:** Paragon assets are free to use in Unreal projects. Keep the raw `.fbx` **out of public GitHub**: put it in `Art/Characters/` and add `Art/Characters/` to `.gitignore`.

---

## Friday 13 Nov: harness design research and monster check

**A. Harness moodboard (portfolio gold: it shows design process)**
1. Collect 10 to 15 reference images: Attack on Titan ODM gear **cosplay builds** (real straps, buckles, hip boxes), climbing harnesses, and parachute harnesses.
2. Make a one-page board (in Claude Design, Canva or Photoshop). Mark what you're keeping:
   - where the straps cross;
   - the hip gas canisters;
   - where the wires leave the body (hips, so the rope pulls from your centre of mass, like the physics).
3. Sketch your version on paper or in Blender. Save it to `Docs/media/harness/`.

**B. Meet your monsters**
1. Content Drawer, then `InfinityBladeAdversaries`. Double-click a creature's skeletal mesh, then click the **Animation** tab (top-right).
   - You should see its attack, walk and death animations.
2. Do the same for `ParagonGrux`.
3. Pick **one** to be the first PvE enemy and write it in `NOW.md` under "Decided".
4. Open `Realistic_Starter_VFX_Pack_Vol2` and double-click the effects to preview them. Note which blood or impact effects look right.

Commit with the message: `Harness references and first enemy choice`.

---

## Saturday 14 Nov: play-test with data (be the scientist)

1. Install the analysis tools (once). Open **Command Prompt** and type:
   `pip install pandas numpy matplotlib scipy`
2. In Unreal, **Play** and swing around for 2 to 3 minutes. Try the hooks, auto-pull, Shift to swing and boost. Then press **Esc**.
   - A recording is saved in `Saved/Tests/`.
3. In Command Prompt:
   ```
   cd "C:\Users\User\Documents\Unreal Projects\GrappleRunner"
   python Tools\analyse_telemetry.py Saved\Tests\<newest telemetry file>.csv _mine
   ```
   - It prints the measurements and saves charts into `Docs/media/tests/`.
4. Write a short **T-16: play-test** entry in `Docs/TESTS.md`:
   - your peak "felt" g;
   - frames over 3 g;
   - anything that felt sickening or great, and at what moment.

   Comfort is the point of your suit, so your own observations count as data.

Commit with the message: `T-16 play-test analysis`.

---

## Sunday 15 Nov: portfolio day

1. Open `Docs/DEVLOG.md` and write this week's entry in your own words: what you built, what went wrong (the blackout counts), and what you learnt.
2. Update `README.md`: put your best screenshot or GIF at the top.
3. Commit with the message: `Week 2 devlog and README` and **Push**.
4. Plan next week: three lines in `NOW.md` under "Next".

---

## How to commit (GitHub Desktop)

1. Open **GitHub Desktop**. Top-left, **Current repository** should be `GrappleRunner`.
   - If it isn't: File, then Add local repository, then `C:\Users\User\Documents\Unreal Projects\GrappleRunner`.
2. On the left you'll see the changed files, ticked. Check that no big pack folders appear (Paragon, Infinity Blade and so on are deliberately ignored).
3. Bottom-left **Summary**: type the message, then click **Commit to main**.
4. Top: click **Push origin**.
   - Your commit is now on github.com/Grey-u/GrappleRunner under **your** name.

**Your first commit** (do it on Tuesday, before anything else) includes your level, the plant and music setup and the `.gitignore` changes. Message: `Track my level, materials and sound effects`.

---

## When Claude's usage resets: what to ask for, in this order

Each is one session. Paste the line as your first message.
1. *"Read NOW.md. Do the first-person body: Kwang (or my Blender version) with legs and hands, and the harness at the hips."*
2. *"Read NOW.md. Make the first PvE enemy: chases me, has health, dies; blood from the VFX pack."*
3. *"Read NOW.md. Sword slicing and realistic gore on that enemy."*
4. *"Read NOW.md. In-world gas gauge on the hip gear, and the speed vignette."*
5. *"Read NOW.md. Plants and trees performance (F8): LODs and cull distances, then re-run gr.Test perf."*

**Console commands for your tests** (press **`** while playing):

| Command | What it does |
|---|---|
| `gr.Test perf` | Frame-time test, 10 s, same view every time |
| `gr.Test snap soft` | Rope snap test (try `stiff` to compare) |
| `gr.Test catch soft` | Short-rope hard catch (the old F6 bug) |
| `gr.Test swing soft` | 15 m pendulum with exaggeration |
| `gr.Test swingreal stiff` | The same pendulum, real physics only |

Settings you can tweak without code: click the **GrappleRig** in the Outliner, then the Details panel.
- **Swing Feel:** `SwingExaggeration`, wind settings.
- **Rope Feel:** soft rope settings.
- **Hooks:** `bAutoPull`, `ReelSpeed`.
