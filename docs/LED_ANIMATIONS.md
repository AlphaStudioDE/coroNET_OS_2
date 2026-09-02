# LED Animation Catalog

This catalog explains the selectable LED animations currently present in coroNET OS 2. It is written for builders and users, while the implementation in `src/led/` remains the technical source of truth.

## How To Read The Catalog

coroNET uses 60 SK6812 RGBW LEDs divided into four logical sections:

| Section | Physical LEDs | Purpose |
| --- | --- | --- |
| Right | 0-10 | Right side of the status bar |
| Center | 11-30 | Main front/status area |
| Left | 31-41 | Left side of the status bar |
| Inside | 42-59 | Rear ambient illumination |

Animations draw through logical section and visual-path helpers. This keeps their direction correct when LED mirroring is enabled and prevents individual effects from hard-coding the physical strip layout.

The following behavior is shared by the entire catalog:

- **Inside White** keeps the Inside section white, independent of the selected animation.
- **Inside Ambient** derives a spatially matched aura from nearby outer LEDs.
- **Color Remix** rotates decorative hues while preserving semantic data colors such as filament and temperature colors.
- **Brightness and DIMM** are applied after rendering, independently for each section.
- **Preview** uses representative printer data so telemetry-aware animations remain visible before a print starts.
- Smooth frame blending prevents abrupt transitions between status animations and previews.

## Catalog Status

| Category | Animations | Status |
| --- | ---: | --- |
| Print | 53 | Rebuilt and hardware-tested on the OS 2 engine |
| Pause | 50 | Rebuilt and hardware-tested on the OS 2 engine |
| Error | 51 | Rebuilt and hardware-tested on the OS 2 engine |
| Idle | 50 | Rebuilt on the OS 2 engine; final hardware pass in progress |
| Finish | 50 | Rebuilt and hardware-tested on the OS 2 engine |
| Other | 50 | Rebuild in progress; first five reviewed sets implemented on the OS 2 engine |

## Print

Print animations may use progress, active-tool temperature, bed temperature, chamber temperature, active filament color, the colors of up to four loaded filaments, elapsed time, ETA, printer connectivity, telemetry age, and ventilation fail-safe state.

| # | Animation | What it shows |
| ---: | --- | --- |
| 1 | Progress Bar | Left and Right show the active filament; Center fills with print progress in a contrasting color. Near-black filament falls back to a moving rainbow so progress remains visible. |
| 2 | Laser | Filament-colored sides frame a narrow scanning laser on Center. The beam is red by default and changes to green for red or red-adjacent filament. |
| 3 | Wave | A contrasting wave travels through the completed portion of Center while the sides retain the active filament color. |
| 4 | Thermal | Center displays filament-colored progress. The sides form a chamber-temperature field from cold blue at 20 C to hot red at 60 C, with faster breathing outside the normal range. |
| 5 | Stripes | The completed Center area flickers in stripes made from up to four loaded filament colors. Missing slots receive distinct fallback hues. |
| 6 | Progress Pulse | A luminous progress field emphasizes the exact current position with a stable local marker while the surrounding light pulses. |
| 7 | Comet | A filament-colored comet and fading tail travel around the complete outer visual path, making ongoing motion readable from any side. |
| 8 | Active Section | Progress divides Center into completed and pending work while animated side activity makes the currently running state unmistakable. |
| 9 | Running | A compact runner repeatedly crosses the completed Center span instead of illuminating the whole bar uniformly. |
| 10 | Breathe | All outer sections breathe gently in the active filament color as a low-distraction indication that printing continues. |
| 11 | Wipe | Center behaves like a curtain that closes symmetrically from its middle as progress rises. Side filament light moves with a soft fabric-like wave. |
| 12 | Shimmer | Fine, deterministic highlights shimmer over a subdued filament-colored outer path without changing the underlying material identity. |
| 13 | Bicolor | Filament and its contrasting partner meet and move across Center, creating two clearly separated color fields. |
| 14 | Thermometer | The active nozzle temperature selects the Center color. Completed progress is brighter in the same hue, while side meters represent bed and chamber temperature. |
| 15 | Snake | A growing filament-colored snake consumes the outer path according to progress. At completion it performs a dedicated overfed burst before the Finish state is allowed through. |
| 16 | Rainbow Progress | A clean rainbow occupies only the completed share of the outer path, preserving the attractive original fill concept as its own animation. |
| 17 | Heartbeat | A double-beat filament pulse runs across the three outer sections, turning an otherwise static print indication into a living rhythm. |
| 18 | DNA Helix | Filament and complementary strands weave around one another across the visual path like a rotating double helix. |
| 19 | Pixel Rain | Colored droplets fall through the completed Center region while the sides provide a restrained filament-colored frame. |
| 20 | Orbit | Several points orbit the complete outer path at different offsets, leaving the rest of the strip dark and spatially readable. |
| 21 | Extruder Spark | Nozzle temperature controls the heat tone and spark intensity. Center still carries progress while hot-tool activity appears as controlled sparks. |
| 22 | Layer Scan | A scanning head moves through the completed Center region and sends matching echoes through the side path, resembling a layer inspection pass. |
| 23 | Heat Ripple | Tool, bed, and chamber temperatures seed differently colored ripples that travel across the outer path; Center remains constrained by progress. |
| 24 | Filament Comets | Up to four loaded filament colors become independent moving comets. Unused material slots receive separated fallback colors. |
| 25 | Progress Theater | Marching bulbs animate the completed Center length, while filament and contrasting accents keep the progress boundary easy to locate. |
| 26 | Nozzle Trace | A heat-colored tool marker travels over filament-colored progress and leaves a short fading trace, like a toolhead pass. |
| 27 | Build Plate | Bed temperature drives the thermal base while Center gradually builds a filament-colored object above it according to progress. |
| 28 | Micro Steps | Alternating side phases mimic stepper coils; Center acts as a progress ruler with a moving contrasting tick. |
| 29 | Flow Wave | A continuous filament-flow wave travels around the outer path, with Center intensity limited by the completed print share. |
| 30 | Toolhead Orbit | The active tool selects one of the loaded-material colors. A temperature-tinted toolhead orbits a progress-aware material path. |
| 31 | Thermal Balance | Left meters bed temperature, Right meters chamber temperature, and Center blends both fields around a moving thermal balance point while retaining progress brightness. |
| 32 | Material Core | Two contrasting material cores travel inward and outward across Center over a quiet filament-colored progress bed. |
| 33 | Heat Soak | Chamber temperature determines both color and how far a breathing thermal field advances symmetrically from the outer ends. |
| 34 | Stability Monitor | Samples nozzle and chamber changes over time. Stable readings stay teal; excessive movement produces orange warning activity without hiding progress. |
| 35 | Layer Engine | Side coils sequence like a three-phase motor while an active layer marker travels through the completed Center region. |
| 36 | Time Tunnel | Elapsed time and ETA control opposing filament and complementary streams, turning the whole path into a tunnel between time spent and time remaining. |
| 37 | Chamber Aura | Chamber temperature defines the surrounding thermal aura; filament-colored Center progress remains visible inside it. |
| 38 | Filament Flow | Loaded material colors move as packets through the outer route. The active tool determines the leading color and Center is gated by progress. |
| 39 | Process Stack | Progress, thermal state, and contrasting process layers are stacked into distinct bands so several aspects of the print remain readable at once. |
| 40 | Health Beacon | Connectivity, stale telemetry, and ventilation fail-safe state select a healthy or warning beacon while Center continues to show progress. |
| 41 | Finish Pressure | After 80 percent, contrasting light compresses inward with increasing speed and intensity, building visual tension toward completion. |
| 42 | Dual Temp Meter | Left meters chamber temperature, Right meters active-tool temperature, and the two measurements meet as separate shimmering halves of Center. |
| 43 | Layer Pulse | A ring expands from the exact progress marker through the completed Center area; quiet filament-colored sides answer each cycle. |
| 44 | Toolpath Echo | A toolhead scans back and forth through printed progress while delayed echoes appear at corresponding positions on both sides. |
| 45 | Thermal Ribbon | Bed and nozzle temperatures form opposite ends of a folded thermal ribbon. Center blends them and reveals only the completed progress length. |
| 46 | Infill Grid | Two moving diagonal patterns cross to form an infill-like grid. Center grid visibility is clipped to actual print progress. |
| 47 | Filament Beads | Material-colored beads circulate around the path, while additional beads become fixed in Center as progress advances. |
| 48 | Time Flow | Left represents elapsed time, Right represents estimated remaining time, and Center retains print progress; a droplet moves between both time fields. |
| 49 | Stepper Ticks | Side phases step in opposite order like synchronized motors. Center is a graduated progress ruler with a moving contrasting tick. |
| 50 | Calm Build | A low-motion filament breath surrounds softly terraced Center progress for long prints and quiet environments. |
| 51 | Quality Guard | Validates printer connectivity, telemetry age, temperatures, and ventilation fail-safe state. Healthy operation is teal; suspicious data becomes an orange alert scan. |
| 52 | Nozzle Heat | Tool temperature creates a heat zone around the exact progress marker. Side meters combine nozzle heat and filament identity. |
| 53 | Layer Fill | Center fills continuously rather than in abrupt LED steps; the newest layer edge glows warmer while gently moving filament light remains on both sides. |

## Pause

Pause animations preserve the current print-progress context where it adds meaning, but deliberately slow the visual tempo. Temperature-aware choices help distinguish a safe wait from a machine that is still holding heat.

| # | Animation | What it shows |
| ---: | --- | --- |
| 1 | Amber | Quiet amber side breathing with a restrained Center hold marker. |
| 2 | Hazard | Alternating amber warning blocks identify a paused machine as an area requiring attention. |
| 3 | Freeze | Cool, crystalline Center pixels hold the last progress position as if the print were frozen in time. |
| 4 | Radar | A slow sweep circles the outer path and crosses a fixed Center reference point. |
| 5 | Heartbeat | A deliberately slow double amber beat confirms that the controller is alive while work is suspended. |
| 6 | Progress Bar | Preserves the paused print percentage on Center and adds a soft amber breath on the side sections. |
| 7 | Crossfade | Amber and cool blue trade places gradually across Left, Center, and Right. |
| 8 | Phase | Several low-intensity Center phases advance in sequence without suggesting active printing. |
| 9 | Yellow-White | Warm amber drifts toward neutral white and back, producing a clean service-light effect. |
| 10 | Watchful Eyes | Two symmetric side points open, hold, and close like a machine waiting for operator input. |
| 11 | Amber Strobe | Short amber groups travel around the outer path with generous dark intervals rather than a continuous alarm flash. |
| 12 | Zigzag | An angular amber marker bounces through Center in a repeating wait pattern. |
| 13 | Neon | Soft cyan and amber neon rails breathe independently around a dim Center core. |
| 14 | Hourglass | Light drains from one side and accumulates on the other, then reverses, while Center marks the narrow neck. |
| 15 | Amber Wave | A broad amber wave travels across the complete outer visual path. |
| 16 | Bounce | A single warm point bounces from end to end with a short, smoothly fading tail. |
| 17 | Slow Comet | One long, slow amber comet circles the outer path for a calm but clearly active hold state. |
| 18 | Spinner | A compact spinner rotates around the outer route while Center remains dimly anchored. |
| 19 | Morse Wait | Repeating short and long light groups spell a recognizable waiting rhythm rather than random blinking. |
| 20 | Blue Breathe | A cool blue Center breath provides a calmer alternative to the default amber warning language. |
| 21 | Soft Hold | Filament-aware progress rests under an extremely gentle amber and blue breathing envelope. |
| 22 | Amber Theater | Evenly spaced amber bulbs chase through Center like a slow theater marquee. |
| 23 | Breathing Dots | Separated Center dots breathe out of phase, keeping the pattern light and spacious. |
| 24 | Waiting Ripple | Repeated rings leave Center and travel outward along the visual path. |
| 25 | Parking Lights | Symmetric Left and Right marker lights blink with the measured cadence of parked machinery. |
| 26 | Dim Sparks | Rare, deterministic amber sparks appear over a nearly dark outer background. |
| 27 | Slow Scan | A broad low-speed scan moves across the three outer sections and pauses at its turnaround points. |
| 28 | Frozen Gold | Fixed gold fragments glint occasionally around a frozen Center progress field. |
| 29 | Clock Tick | A regular Center tick and alternating side markers make elapsed waiting time perceptible. |
| 30 | Calm Orbit | A softly blurred amber point orbits a dark Center anchor at a relaxed speed. |
| 31 | Holding Pattern | Left and Right run mirrored aviation-style holding loops around a quiet Center. |
| 32 | Breathing Amber | The whole outer bar breathes slowly in warm amber with spatial phase offsets. |
| 33 | Resume Gate | Two Center gates breathe and gradually open, visually preparing the route for a resume action. |
| 34 | Temp Keepalive | Left, Center, and Right meter chamber, bed, and nozzle temperature so heat retained during the pause remains visible. |
| 35 | Soft Attention | Subtle side pulses ask for attention without using an error-level flash pattern. |
| 36 | Operator Wait | A stable Center request marker is answered alternately by Left and Right, suggesting a pending human action. |
| 37 | Frozen Layer | Retains the current progress boundary as a cool frozen layer with sparse crystalline highlights. |
| 38 | Filament Hold | Filament-colored packets stop at a Center hold point instead of continuing through the path. |
| 39 | Do Not Touch | Strong symmetric amber edge markers and a guarded dark center communicate that the machine should remain undisturbed. |
| 40 | Heat Hold Split | Chamber and nozzle temperatures occupy opposite sides and meet at Center while the paused progress remains marked. |
| 41 | Calm Down | Broad waves lose energy as they move away from Center, creating a visibly settling animation. |
| 42 | Still Water | A very dim progress reflection sits under slow, low-amplitude ripples that travel across the path. |
| 43 | Soft Lantern | A warm Center lantern breathes with tiny deterministic variations and faint side spill. |
| 44 | Hold Orb | A rounded light mass expands and contracts around Center, with matching low side illumination. |
| 45 | Suspended Layer | The completed layer appears suspended above a dark pending region, held by quiet side supports. |
| 46 | Gentle Reminder | Widely spaced amber reminders appear periodically, leaving most of the cycle calm. |
| 47 | Breath Gate | Two Center halves breathe together like a closed but ready doorway. |
| 48 | Waiting Room | Several separated Center seats illuminate in a slow sequence, making the wait state playful without looking busy. |
| 49 | Tool Park | The active tool selects the parked marker position; Left and Right breathe gently around it. |
| 50 | Resume Ramp | Light gradually ramps through the outer path and then settles, previewing the direction of a future resume transition. |

## Error

Error animations range from unmistakable emergency signals to diagnostic patterns. Telemetry-aware entries react to network availability, stale printer data, ventilation fail-safe state, chamber temperature, bed temperature, and active-tool temperature.

| # | Animation | What it shows |
| ---: | --- | --- |
| 1 | Blink | A direct full-bar red on/off warning with no decorative motion. |
| 2 | SOS | The complete outer bar transmits the SOS short-short-short, long-long-long, short-short-short rhythm. |
| 3 | Alarm | Double amber side flashes surround a restless red Center core. |
| 4 | Critical | A long red warning hold ends with a blackout and a final maximum-intensity flash. |
| 5 | Police | Blue Left and red Right alternate in paired flashes; Center is split into matching halves. |
| 6 | Red Breathe | A spatially offset red breath rolls through all outer sections. |
| 7 | Heartbeat | A sharp double red heartbeat repeats across the entire outer bar. |
| 8 | Strobe | Alternating three-LED groups produce four brief red flashes followed by a dark recovery interval. |
| 9 | Red Wave | Two red waves of different speed cross one another around the outer path. |
| 10 | Xenon | Two extremely short cold-white flashes reproduce the cadence and color of a xenon warning beacon. |
| 11 | Siren | A white-tipped red siren beam runs continuously around the outer path with a long tail. |
| 12 | Thunder | Deterministic branched cold-white strikes hit changing sections over a nearly dark red background. |
| 13 | Countdown | Red pulses accelerate while the illuminated region contracts toward Center. |
| 14 | Glitch | Deterministic red noise, amber faults, and rare white corruption marks create a repeatable digital failure effect. |
| 15 | Alarm Chase | Moving red and white warning bands chase around the complete outer route. |
| 16 | Danger Stripe | Animated amber and red hazard stripes move along the visual path. |
| 17 | Pulse Alert | A bright red ring repeatedly expands from Center with a fading background pulse. |
| 18 | Redout | Red visibility collapses toward and away from Center like a system losing power. |
| 19 | Emergency | Bright white blocks cut through moving red groups on every section. |
| 20 | Meltdown | Dense red-to-amber heat noise rises through each section like an uncontrolled thermal event. |
| 21 | Crash | A bright impact ring bursts outward, leaves a blackout, and returns as a low red Center warning. |
| 22 | Red Theater | Spaced red warning bulbs chase around the path with a softer afterglow. |
| 23 | Fault Ripple | A repeatable pseudo-random fault point emits a circular red ripple and remains marked after the wave passes. |
| 24 | Hot Zone | Left meters chamber heat, Center meters bed heat, and Right meters tool heat; the hottest reading also increases pulse urgency. |
| 25 | Panic Comets | Three red and amber comets of different speed and tail length race around the outer path. |
| 26 | Lockdown | Red gates close inward from both ends and latch with a flashing Center lock. |
| 27 | Warning Ticks | Sparse red ruler marks surround the bar while one amber tick advances at a measured pace. |
| 28 | Breach Scan | A scanner searches the whole path, finds a changing breach position, and holds it with an amber blink. |
| 29 | Fault Sparks | Short deterministic sparks break away from a low red background at changing positions. |
| 30 | Red Juggle | Several red points follow independent oscillating paths, crossing without turning into a full-bar flash. |
| 31 | Evacuate | Directional warning blocks move away from Center toward both exits. |
| 32 | Cause Hint | Left indicates a network-related fault, Right indicates a thermal/fail-safe fault, and Center represents an unidentified cause. |
| 33 | Stack Light | Left, Center, and Right act as a three-stage red, amber, and status stack lamp. |
| 34 | Smart Heartbeat | Fault severity determines heartbeat speed and color; combined network and thermal issues create the most urgent rhythm. |
| 35 | Location Split | Left identifies connectivity state, Right identifies thermal state, and a red Center scanner ties both diagnostics together. |
| 36 | Blackout Flash | A nearly complete blackout is interrupted by two hard white flashes and a faint late red return. |
| 37 | Recovery Wait | Amber circulates while a fault remains; the same route becomes green once network and thermal conditions recover. |
| 38 | Siren Scan | A broad red siren beam scans from one end of the complete path to the other and back. |
| 39 | Diagnostic Bits | Four repeated Center bit groups encode network, thermal, ventilation fail-safe, and very stale telemetry conditions. |
| 40 | Service Beacon | An amber ring expands from Center around a persistent red service point. |
| 41 | Safe Shutdown | Red light withdraws progressively toward darkness, followed by a small final amber shutdown acknowledgement. |
| 42 | Calm Alert | Slow red side breathing and a shaped amber Center field provide an error indication suitable for less urgent faults. |
| 43 | Fault Locator | A red scanner searches the path and latches onto a blue network, amber thermal, or central unknown-fault location. |
| 44 | Thermal Cut | The hottest available nozzle or chamber reading controls color, pulse speed, and the position of a visible break in the thermal path. |
| 45 | Network Lost | Blue packet groups travel along the path; deterministic red drops interrupt them to visualize a broken data link. |
| 46 | Service Code | One to four amber flashes and matching Center marks encode unknown, network, thermal, or combined fault classes. |
| 47 | Containment | Red barriers close from both edges of Center and latch with a pulsing central seal. |
| 48 | Safe Breath | A low-intensity spatial red breath keeps the error visible without rapid flashes. |
| 49 | Escalation | Over six seconds the red/amber pulse becomes faster and brighter, ending with a rapid white tracer before resetting. |
| 50 | Repair Beacon | Amber orbit and red heartbeat indicate an unresolved fault; both become green when monitored conditions recover. |
| 51 | Cooling Alarm | Cool blue and chamber-temperature heat approach from opposite sides and blend through Center; higher heat increases motion speed. |

## Idle

Idle animations provide useful ready-state cues and lower-motion ambient scenes. All 50 coroNET 1 concepts have been rebuilt around the OS 2 section mapping and global LED policies.

| # | Animation | What it shows |
| ---: | --- | --- |
| 1 | Rainbow | A complete, slowly rotating spectrum follows the full visual OUTER route with a restrained spatial shimmer. |
| 2 | Fireplace | Independent layered flames move through both sides while Center holds a lower amber ember bed. |
| 3 | Ocean | Two differently paced cyan and blue wave fields combine into broad swells and smaller surface ripples. |
| 4 | Star Pulse | A softly shaped star-like field breathes from Center while its color advances only once per long cycle. |
| 5 | Meteor | A saturated meteor with a long fading tail crosses the complete OUTER path, rests in darkness, and returns in a new hue. |
| 6 | Twinkle | Sparse warm-white stars appear and decay deterministically over darkness without a retained particle buffer. |
| 7 | Larson | A classic saturated red scanner bounces end to end across all 42 OUTER LEDs with a long directional tail. |
| 8 | Lava | Two slow opposing flow fields form moving hot orange, red, and magenta molten regions. |
| 9 | Gradient | A broad two-color gradient spans the complete outer path while both endpoint hues drift together. |
| 10 | Plasma | Two faster interference fields drive both hue and brightness, creating a continuous energetic plasma surface. |
| 11 | Section Breathe | Left, Center, and Right breathe in a slow three-phase amber sequence instead of changing together. |
| 12 | Snow | Cool-white flakes descend independently through each physical section over a faint winter-blue field. |
| 13 | Color Wipe | One saturated color fills the complete visual path pixel by pixel, holds, erases cleanly, and returns in a new hue. |
| 14 | Moonlight | A broad pale-blue moon halo circles slowly over a subtly rippling low-light background. |
| 15 | Tetris | Small discrete multicolor blocks of several lengths travel through the complete OUTER route with visible gaps. |
| 16 | Running | A repeating two-pixel runner moves continuously around OUTER while its shared saturated hue changes slowly. |
| 17 | Bubbles | Cool bubbles rise independently through both sides and produce small white-blue pops at the edges of Center. |
| 18 | Drift | A broad low-saturation aurora ribbon drifts slowly through hue and brightness across the entire route. |
| 19 | Candle | Independent warm amber flicker combines a slow flame envelope with bounded deterministic variation per LED. |
| 20 | Starfield | Every LED behaves as an independently timed, low-intensity cool star, creating a continuously living sky rather than sparse flashes. |
| 21 | Aurora Ribbon | Two broad green, cyan, and violet ribbons move at different speeds and brighten where their folds cross. |
| 22 | Rainbow Glitter | A restrained moving rainbow carries rare warm-white glints without replacing its underlying color field. |
| 23 | Soft Comet | One slow cyan-violet comet circles OUTER with a very long low-saturation tail and an almost-dark background. |
| 24 | Kaleidoscope | The two halves of OUTER mirror six repeating color facets exactly while their shared pulse rotates. |
| 25 | Breathing Orbit | Two opposite cool orbiters circle the complete path while their heads and tails breathe together. |
| 26 | Pixel Fireflies | Six warm yellow-green lights glide between deterministic targets instead of following a fixed strip direction. |
| 27 | Cosmic Dust | A dense but very dim violet-blue grain field drifts slowly through a broad cosmic cloud. |
| 28 | Theater Glow | Soft cyan marquee bulbs chase in a five-pixel rhythm over a low breathing auditorium glow. |
| 29 | Tidal Pool | A separate aqua ripple expands from the center of each physical section, producing three offset pools. |
| 30 | Neon Drift | Wide cyan and magenta neon bands move in opposite directions and blend only where they cross. |
| 31 | Ready Breath | Printer availability selects cyan ready or amber waiting light; the sides breathe while Center holds a softer confirmation field. |
| 32 | Ambient Clock | Minute and second positions are mapped around OUTER as a gold marker and a moving cyan hand, with an uptime fallback before time sync. |
| 33 | Temp Idle | Mirrored side meters and a Center thermal gradient display the current chamber temperature from 20 to 65 C. |
| 34 | Last Print Echo | The remembered filament color and progress remain visible while a soft highlight repeatedly scans the completed Center bar. |
| 35 | WiFi Beacon | Amber, cyan, or green identifies no Wi-Fi, Wi-Fi without printer telemetry, or a complete online path; orbit speed reinforces the state. |
| 36 | Sleepy Core | Almost-dark blue sides surround a very slow violet Center core for the lowest-motion decorative idle scene. |
| 37 | Material Shelf | Four separated OUTER compartments show the printer's loaded filament colors, with distinct restrained fallbacks for empty slots. |
| 38 | Status Ring | Left reports Wi-Fi, Center reports printer connection, and Right reports telemetry freshness using independently meaningful colors. |
| 39 | Chamber Lantern | Actual chamber temperature colors a low breathing side aura and a brighter symmetric Center lantern. |
| 40 | Ready Split | Loaded filament color frames both sides while Center scans green when the printer is available or amber while waiting. |
| 41 | Calm Tide | Two long aqua and blue-green swells cross slowly around the complete OUTER route without sharp peaks or flashes. |
| 42 | Zen Garden | Each physical section forms its own sand or moss bed while two softly lit stones wander slowly within its boundaries. |
| 43 | Dusk Horizon | Night blue and sunset orange meet at a slowly breathing horizon independently inside Left, Center, and Right. |
| 44 | Silk Flow | Opposing violet and turquoise folds blend into a flowing ribbon, with a restrained pale sheen only where folds overlap. |
| 45 | Northern Sleep | Six wide green-to-violet aurora curtains sway over an almost-dark blue sky at independent, deliberately slow rates. |
| 46 | Dew Sparks | A low aqua morning mist carries occasional droplets that brighten and fade smoothly instead of flashing on for one frame. |
| 47 | Lamp Glow | Every physical section becomes a warm spatial lamp: brightest at its own center, softer at its edges, and breathing as one scene. |
| 48 | Cloud Drift | Three broad, differently paced wave fields create cool cloud masses whose density changes continuously along OUTER. |
| 49 | Quiet Comet | One broad, tail-free cyan comet crosses the route once, leaves a long dark pause, and then begins another quiet passage. |
| 50 | Section Calm | Left, Center, and Right share one guided breath while retaining separate muted green, cyan, and violet bell-shaped light fields. |

## Finish

Finish animations celebrate a completed job. The complete rebuilt set combines show-oriented scenes with filament-aware, cooldown, inspection, and deliberately calm choices.

| # | Animation | What it shows |
| ---: | --- | --- |
| 1 | Sweep | A white-tipped green and gold success beam completes a victory lap around the entire outer path. |
| 2 | Rainbow | Each physical section presents a complete moving spectrum with smooth tonal offsets at the section boundaries. |
| 3 | Pulse | A warm white and gold victory ring repeatedly expands from Center over a softly breathing background. |
| 4 | Filament | The finished material color becomes a flowing illuminated surface with restrained white crests; near-black filament receives a color-cycle fallback. |
| 5 | Fireworks | A deterministic gold launch climbs the path and opens into a different multicolor circular burst each cycle. |
| 6 | Curtain | A textured gold curtain opens symmetrically across Center while Left and Right provide softly moving stage light. |
| 7 | Confetti | Independent colored pieces appear and decay across the complete outer path without retaining frame-sized particle buffers. |
| 8 | Gold Rain | Offset gold droplets travel through all three sections with bright heads and a low afterglow. |
| 9 | Strobe Party | Three sections answer a syncopated celebration beat with different saturated colors and brief dark rests. |
| 10 | Bouncing Balls | Four independently timed colored balls traverse and rebound across the complete 42-LED OUTER route, rather than being confined to Center. |
| 11 | Rainbow Explosion | A spectrum shell expands from Center, leaves a dim colorful wake, and fades before the next explosion. |
| 12 | Disco | Held three-LED color blocks alternate on a syncopated beat, producing deliberate stage cuts instead of random full-section changes. |
| 13 | Heart | A shaped rose-colored double heartbeat is brightest at Center and remains visible as a soft outline between beats. |
| 14 | Color Spiral | A rotating hue helix winds continuously around the outer visual path with a wave-shaped brightness profile. |
| 15 | Sparkle | Warm white glints appear over a quiet active-filament surface, like light catching the completed print. |
| 16 | Champagne | Gold bubbles rise independently through both sides and burst as bright points across Center. |
| 17 | Wipe Out | Filament and gold light wipe inward from both ends, meet, and withdraw in a continuous cycle. |
| 18 | Fill | Gold advances smoothly one LED at a time around the full path, then holds as a moving completed shimmer. |
| 19 | Waterfall | Three cool color streams cascade around the full outer route with long fading tails. |
| 20 | Starburst | A compact white core emits a fast multicolor shell, then the complete burst decays to darkness. |
| 21 | Victory Lap | A filament-colored runner led by green light circles the full path and repeatedly crosses a gold-and-white checkered finish gate. |
| 22 | Gold Theater | A classic warm marquee chases around the complete bar with a bright bulb and visible afterglow. |
| 23 | Ribbon Dance | Filament and gold ribbons travel in opposite directions, cross, and trade visual dominance without becoming a single blended field. |
| 24 | Trophy Glow | Center forms a bright gold cup and pedestal while mirrored side handles breathe around it. |
| 25 | Star Glitter | Warm stars twinkle independently over a very dark blue night field, clearly separated from the filament-based Sparkle scene. |
| 26 | Dual Comets | Filament and gold comets travel in opposite directions with long tails and repeatedly pass one another around OUTER. |
| 27 | Applause | Two gold waves clap inward from opposite ends in paired beats and meet in a short white Center impact. |
| 28 | Prism Bloom | White light opens slowly from Center and separates into increasingly saturated colors toward both ends before fading. |
| 29 | Pixel Toast | Gold pixels rise through Center, settle into a warm completed surface, and briefly pop filament-colored markers onto the sides. |
| 30 | Crown Chase | Center forms a five-point gold crown while a changing jewel and mirrored side highlights travel around it. |
| 31 | Cooldown Progress | Left meters chamber heat, Right meters nozzle heat, and Center fills toward a green ready state as the hottest reading falls. |
| 32 | Print Signature | The completed filament color fills Center while a white-tipped signature highlight repeatedly circles the full result. |
| 33 | Smart Applause | Paired gold claps become faster and stronger for longer completed jobs, with the brightest impact at Center. |
| 34 | Take Me | Green arrows move inward toward a held Center target to signal that the finished object is waiting for the user. |
| 35 | Cool To Touch | The hotter of bed and nozzle controls warning color and pulse speed; Center fills toward green as the machine becomes safe to touch. |
| 36 | Last Layer Glow | A textured filament-colored ridge glows across the complete Center while matching side reflections move more softly. |
| 37 | Gallery Mode | Neutral warm-white Center illumination presents the finished object while dim filament-colored sides provide a restrained frame. |
| 38 | Filament Fireworks | Firework shells cycle through up to four loaded filament colors, using separated fallback hues for empty or near-black slots. |
| 39 | Inspection Light | A neutral high-visibility beam scans back and forth across Center while both sides remain steadily illuminated. |
| 40 | Quiet Pride | A calm filament breath surrounds a stable gold Center arch for a low-motion completion scene. |
| 41 | Calm Done | A very slow filament-and-green settle keeps the completed state visible without demanding attention. |
| 42 | Silk Unveil | Two broad interference folds travel through the filament color and catch a restrained pearl sheen like moving silk. |
| 43 | Golden Hour | A spatial amber-to-gold horizon surrounds a brighter Center sun with slow natural light movement. |
| 44 | Starfall | Sparse cool-white and warm stars fall symmetrically from both outer ends toward Center over a dark night field. |
| 45 | Signature Sweep | A white-tipped filament stroke writes across Center and dries into a finished signature between two gold seals. |
| 46 | Inspect Ready | A neutral inspection beam checks Left, Center, and Right in sequence, then holds a green all-clear presentation. |
| 47 | Print Echo | Three alternating filament and gold echoes repeatedly travel outward from the completed object's Center. |
| 48 | Soft Applause | Two broad low-intensity gold waves approach from the ends, meet gently, and fade without a hard flash. |
| 49 | Cooldown Aura | Nozzle and bed heat continuously move a breathing full-bar aura from hot thermal color toward cool cyan. |
| 50 | Showcase Loop | A seamless four-scene exhibition loop crossfades through filament reveal, gold orbit, prism light, and gallery illumination. |

## Other

Other is independent of printer status and is intended for manually selected ambient scenes. The large coroNET 1 collection is being rebuilt in reviewed sets so similarly named effects retain clearly different motion and composition.

| # | Animation | What it shows |
| ---: | --- | --- |
| 1 | Matrix | Three independent green code streams descend through every physical section, each with a bright head and decaying digital trail. |
| 2 | Candle | Left, Center, and Right become three spatial candle flames whose warm cores, edges, and bounded flicker remain individually shaped. |
| 3 | Static Rainbow | A complete stationary spectrum is repeated within each physical section, keeping every color visible without motion. |
| 4 | Neon Club | Cyan and magenta bar groups move under a three-section club beat without using full-white strobe flashes. |
| 5 | Synthwave | Cyan and magenta side perspective feeds a receding Center grid with a bright moving horizon line. |
| 6 | Jellyfish | A translucent violet-blue bell drifts independently through each section with several softly fading tendrils behind it. |
| 7 | Snow | Five flakes fall within each section over a cold snow-globe field and gradually collect at the lower edge before the globe resets. |
| 8 | Sunset | A warm sun moves through a red-to-violet sky independently inside all three physical sections. |
| 9 | Volcano | Center forms a breathing molten crater while timed eruptions throw orange sparks symmetrically through both side sections. |
| 10 | Techno | A deterministic 16-step cyan and magenta sequencer moves gated pixels across all sections and emphasizes only its downbeats. |
| 11 | Dragon Blood | A living crimson field pulses around two moving black fissures, replacing random whole-strip blackouts with controlled tension. |
| 12 | Aurora | One seamless green-to-blue curtain crosses the full OUTER route, with violet edge light appearing only at its strongest folds. |
| 13 | Cyberpunk | Cyan and magenta side gates answer one another while a two-color data packet runs along the Center spine. |
| 14 | Nebula | Three differently paced violet and magenta clouds form dense moving gas with rare, softly blended stellar cores. |
| 15 | Submarine | A green sonar sweep crosses Center while pale bubbles rise in opposite physical directions through Left and Right. |
| 16 | Pride | Six ordered rainbow stripes remain recognizable while a restrained spatial wave flexes their boundaries and brightness. |
| 17 | Plasma | Two opposing fields produce dark nodes and bright violet-blue electrical arcs according to their instantaneous difference. |
| 18 | Bouncing Balls | Five differently timed colored balls use smooth acceleration, reach both ends of the complete OUTER path, and retain directional tails. |
| 19 | Cop Car | Timed blue and red side bursts frame a split-color Center sweep without introducing white flashes. |
| 20 | Strobe Party | Two bounded saturated-color hits form each beat while the active section rotates; long dark intervals keep it distinct from a constant strobe. |
| 21 | Sunrise | A warm core rises from the middle of OUTER, pushes night blue toward the edges, reaches a pale-gold morning peak, and recedes slowly. |
| 22 | Ocean Depth | Three bioluminescent creatures circle through a very dark, slowly moving blue current with broad local halos. |
| 23 | Radiation | A yellow-green warning ring repeatedly expands from the middle of the complete OUTER route over a breathing hazard field. |
| 24 | Pastel | Left, Center, and Right carry separate low-saturation color fields whose wide gradients drift without collapsing into one flat hue. |
| 25 | Electric | A fast blue electrical head and short tail cross OUTER while deterministic branch points ignite along its current strike path. |
| 26 | Rainbow Pulse | A complete animated spectrum is repeated in each physical section and breathes through both global and local intensity waves. |
| 27 | Carnival | Red, gold, and blue marquee sections rotate their roles while compact bulb groups chase inside each section. |
| 28 | Neon Sign | Three colored neon tubes follow separate flicker-on, stable, failing, and off stages without using frame-random flashes. |
| 29 | Motion Detect | A real touch launches a cyan ring from Center across OUTER; preview supplies periodic demonstration touches while normal operation remains event-driven. |
| 30 | Retro TV | Deterministic colored analogue noise refreshes at a bounded rate while a brighter scan line travels through the signal. |
| 31 | Crystal | Mirrored facets inside every physical section glint independently in restrained ice-blue and violet tones. |
| 32 | Fire and Ice | Animated fire owns Left, frost owns Right, and both thermal fields collide in a pale breathing seam across Center. |
| 33 | Laser Grid | Red and green emitters launch two Center beams at different speeds; their crossing becomes a compact gold intersection. |
| 34 | Galaxy Spin | Two counter-rotating violet spiral arms orbit a stable bright core and lose energy gradually toward both ends. |
| 35 | Comet Twins | Cyan and magenta comets travel in opposite directions across the complete path with independent directional tails. |
| 36 | Deep Sea Pulse | A high-pressure blue wave advances inward from both ends over a dark, slowly moving deep-water field. |
| 37 | Solar Wind | Long gold and orange particle streams flow in one direction while a slower turbulence field changes their heat tone. |
| 38 | Pixel Circus | Each section takes a turn as the main ring while the other two run different supporting pixel acts and colors. |
| 39 | Mint Breeze | Three offset, low-saturation mint waves drift gently through the physical sections as a quiet decorative scene. |
| 40 | Ruby Scan | A deep-red faceted background carries a saturated ruby scanner that bounces over the entire OUTER route. |
| 41 | Arcade Chase | Four primary arcade pellets run in bright and dim pairs around OUTER, separated by a repeatable dark gap. |
| 42 | Stardust | Seven warm stellar fragments stream outward from the middle over a sparse cool star field and retain short directional trails. |
| 43 | Ice Cave | Mirrored cave walls grow inward from every section edge; fixed facets glint at independent, deterministic intervals. |
| 44 | Firework Trail | A gold launch trail reaches Center, bursts into two multicolor fronts, decays toward both ends, and rests before repeating. |
| 45 | Chroma Ring | A compact 18-pixel ring circles OUTER while its long tail walks continuously through the color spectrum. |
| 46 | Ghost Light | One broad, almost-white blue apparition fades in, glides once across the path, fades away, and leaves a long dark pause. |
| 47 | Toxic Wave | High-saturation acid green ooze and violet poison bubbles move at separate rates and mix into a viscous two-layer field. |
| 48 | Copper Spark | A breathing copper ember bed carries sparse sparks with explicit rise and decay envelopes instead of one-frame flashes. |
| 49 | Blueprint | A dark blue measured grid marks major and minor divisions while a bright cyan drawing pen scans the full route. |
| 50 | Magma Flow | Two molten flow layers move beneath deterministic black crust cracks, shifting continuously through red, orange, and hot yellow. |

## Boot LED Experience

Boot lighting is intentionally not selectable from the normal animation catalog. The first-run experience performs the long, audio-synchronized light show before entering Setup Wizard. Later starts use a shorter silent signature. Both variants hand their final LED frame to the active status animation through a timed crossfade, so normal operation does not begin with an abrupt visual cut.

## Maintenance Rule

When an animation is added, removed, renamed, or substantially redesigned, update this document in the same commit. The animation-name arrays have compile-time count checks; this catalog is reviewed alongside those checks during each completed category migration.
