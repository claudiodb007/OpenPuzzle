# Windows automatic GPU benchmark

The desktop GPU benchmark now chooses threads, blocks and points rather than
asking the user to guess them. CPU measurement retains its explicit thread
count. This update also includes first-profile creation and persistence.

## Shared Linux behavior

Windows uses the same `AutoTuner::defaultMatrix`, memory estimate and 70% memory
budget as the Linux `benchmark --auto` implementation. CUDA includes its 512
thread candidates; OpenCL retains the portable matrix. The selected engine's
native platform/device/name/memory mapping must uniquely match driver discovery
before its compute-unit count can determine the matrix. Unknown capabilities do
not fall back to another GPU or an unbounded launch.

Each GPU candidate runs for 30 seconds in the panel. Measurements discard the
first five seconds and need at least three sustained samples. Selection uses
the shared average-minus-spread stability score. The winner then runs for 60
seconds. Only a successful final measurement produces a profile, with the same
3% conservative planning margin as Linux. This is the best tested candidate,
not proof of a global optimum or of long-term thermal equilibrium.

The CLI exposes `benchmark --backend cuda|opencl --device N --auto --real`.
Without `--real` it is a local preview. `--seconds 10..30` controls each candidate;
the finalist is always 60 seconds. Combining automatic tuning with fixed
threads/blocks/points is rejected. Existing manual CLI benchmarks still work.

## Ownership, cancellation and persistence

No coordinator calls, identity creation, production assignments, power writes,
fan changes or clock changes are part of automatic measurement. The GUI's Stop
button terminates its owned benchmark CLI; the existing engine Job Object kills
its children when the CLI closes. Closing the panel is blocked during owned
work. Errors and cancellation do not publish a completed result.

Failed launch configurations are logged and skipped. Unexpected target output,
invalid/regressing samples, engine replacement, an active runtime, missing or
ambiguous capabilities, or failed finalist validation prevent publication.
Reports and earlier results remain available. The GUI physically binds a
successful GPU result with the existing UUID/PCI verification before selecting
or saving it.

A first manual profile still uses `profiles/<backend>/result.json` without
replacing an existing valid, damaged or occupied destination. Each automatic
profile uses a new `auto-*.json` in that backend's profile directory. An owned
`automatic-selected.txt` pointer makes it discoverable on reopening. Earlier
measurements and the original result.json remain unchanged. Malformed or
occupied automatic selection data is preserved and blocks replacing it.

## Validation boundaries

Portable tests exercise the shared matrix and score, rejected/invalid
candidates, finalist failure, planning margin, cancellation and no safe memory
candidate. Native Windows fixtures cover the GPU UI argument route, automatic
parameter display, missing finalist provenance, failed measurement, UUID/PCI
binding, persistence/reopening, repeated measurements and malformed selection
preservation. Existing process cancellation and engine containment tests remain
part of the isolated suite.

Cross compilation is not native execution. The accompanying validation kit
builds all 43 isolated Windows suites with MSVC and runs them before creating a
preview. It never launches a real benchmark or a production search itself.
Real automatic GPU measurements and native Windows execution remain pending
until the Windows report is supplied.
