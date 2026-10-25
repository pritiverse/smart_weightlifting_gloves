import React, { useState } from 'react';

// ─── Reusable info tooltip ────────────────────────────────────────────────────
const Tip: React.FC<{ text: string }> = ({ text }) => {
  const [show, setShow] = useState(false);
  return (
    <span style={{ position: 'relative', display: 'inline-block', verticalAlign: 'middle', marginLeft: '5px' }}>
      <span
        onMouseEnter={() => setShow(true)}
        onMouseLeave={() => setShow(false)}
        style={{
          width: '15px', height: '15px', borderRadius: '50%',
          background: 'rgba(0,229,255,0.15)', border: '1px solid rgba(0,229,255,0.3)',
          color: 'var(--cyan-primary)', fontSize: '10px', fontWeight: 700,
          display: 'inline-flex', alignItems: 'center', justifyContent: 'center',
          cursor: 'help', userSelect: 'none',
        }}
      >?</span>
      {show && (
        <span style={{
          position: 'absolute', bottom: '22px', left: '50%', transform: 'translateX(-50%)',
          background: '#0d1526', border: '1px solid var(--border-highlight)',
          borderRadius: '8px', padding: '8px 12px', fontSize: '11px', lineHeight: 1.5,
          color: '#cdd6f4', whiteSpace: 'pre-wrap', width: '220px',
          boxShadow: '0 8px 24px rgba(0,0,0,0.5)', zIndex: 999, pointerEvents: 'none',
        }}>
          {text}
        </span>
      )}
    </span>
  );
};

// ─── Section card ─────────────────────────────────────────────────────────────
const Section: React.FC<{ icon: React.ReactNode; title: string; color?: string; children: React.ReactNode }> = ({
  icon, title, color = 'var(--cyan-primary)', children,
}) => (
  <div style={{
    background: 'var(--bg-surface)', borderRadius: '10px',
    border: '1px solid var(--border-subtle)', padding: '16px', marginBottom: '12px',
  }}>
    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '12px' }}>
      <span style={{ color }}>{icon}</span>
      <span style={{ fontSize: '13px', fontWeight: 700, color: '#e2e8f0', textTransform: 'uppercase', letterSpacing: '0.05em' }}>{title}</span>
    </div>
    {children}
  </div>
);

// ─── Row inside a section ─────────────────────────────────────────────────────
const Row: React.FC<{ label: string; value?: string; tip?: string; children?: React.ReactNode; accent?: string }> = ({
  label, value, tip, children, accent = '#fff',
}) => (
  <div style={{
    display: 'flex', justifyContent: 'space-between', alignItems: 'flex-start',
    padding: '7px 0', borderBottom: '1px solid rgba(255,255,255,0.04)', gap: '12px',
  }}>
    <div style={{ display: 'flex', alignItems: 'center', flex: 1, minWidth: 0 }}>
      <span style={{ fontSize: '12px', color: 'var(--text-muted)', flexShrink: 0 }}>{label}</span>
      {tip && <Tip text={tip} />}
    </div>
    {value && <span style={{ fontSize: '12px', fontWeight: 700, color: accent, fontFamily: 'var(--font-mono)', textAlign: 'right', flexShrink: 0 }}>{value}</span>}
    {children}
  </div>
);

// ─── Badge ────────────────────────────────────────────────────────────────────
const Badge: React.FC<{ label: string; color: string; bg: string }> = ({ label, color, bg }) => (
  <span style={{
    padding: '2px 8px', borderRadius: '4px', fontSize: '11px', fontWeight: 700,
    background: bg, color, border: `1px solid ${color}40`, marginRight: '4px', display: 'inline-block',
  }}>{label}</span>
);

// ─── Main component ───────────────────────────────────────────────────────────
export const InfoGuide: React.FC = () => {
  const [open, setOpen] = useState(false);

  return (
    <div style={{
      position: 'fixed', bottom: '24px', right: '24px', zIndex: 500,
      display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: '12px',
    }}>
      {/* ── Slide-up panel ── */}
      {open && (
        <div style={{
          width: '400px', maxHeight: '80vh', overflowY: 'auto',
          background: 'var(--bg-card)', border: '1px solid var(--border-highlight)',
          borderRadius: '14px', boxShadow: '0 24px 64px rgba(0,0,0,0.7)',
          padding: '20px', animation: 'fadeInUp 0.2s ease',
        }}>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '16px' }}>
            <div>
              <div style={{ fontSize: '16px', fontWeight: 800, color: '#fff' }}>📖 Dashboard Guide</div>
              <div style={{ fontSize: '11px', color: 'var(--text-muted)', marginTop: '2px' }}>What every number means</div>
            </div>
            <button onClick={() => setOpen(false)} style={{
              background: 'none', border: 'none', color: 'var(--text-muted)',
              fontSize: '20px', cursor: 'pointer', lineHeight: 1,
            }}>✕</button>
          </div>

          {/* ── WHAT IS A REP? ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><path d="M12 2v20M17 5H9.5a3.5 3.5 0 0 0 0 7h5a3.5 3.5 0 0 1 0 7H6"/></svg>}
            title="What counts as 1 rep?"
            color="var(--cyan-primary)"
          >
            <p style={{ fontSize: '12px', color: '#a0aec0', lineHeight: 1.7, margin: '0 0 10px' }}>
              The glove watches your wrist angle 50 times per second and counts a rep only when <strong style={{ color: '#fff' }}>ALL four rules pass at once:</strong>
            </p>
            <Row
              label="① Range of Motion ≥ 35°"
              tip="Your wrist must travel at least 35 degrees from start to peak. For a bicep curl that means fully extending at the bottom and fully curling at the top."
              accent="var(--emerald-valid)"
              value="≥ 35°"
            />
            <Row
              label="② Grip pressure ≥ 40%"
              tip="You must be squeezing the dumbbell with at least 40% of your firm-grip force (set during calibration). Loose fingers = rejected rep."
              accent="var(--cyan-primary)"
              value="≥ 40%"
            />
            <Row
              label="③ Duration 0.6s – 5.0s"
              tip="The rep must take between 0.6 and 5 seconds. Faster than 0.6s = a jolt/cheat. Slower than 5s = paused/stalled."
              accent="var(--amber-warning)"
              value="0.6 – 5 s"
            />
            <Row
              label="④ Sensors healthy"
              tip="The glove's IMU and grip sensor must both be in HEALTHY state. If a sensor fault is detected, rep counting is automatically gated off."
              accent="var(--emerald-valid)"
              value="No fault"
            />
            <div style={{ marginTop: '10px', padding: '10px', background: 'rgba(0,229,255,0.06)', borderRadius: '8px', fontSize: '11px', color: '#94a3b8', lineHeight: 1.6 }}>
              💡 <strong style={{ color: 'var(--cyan-primary)' }}>Bicep Curl example:</strong> Arm down (0°) → curl up to ~80° → arm back down. The glove detects the peak angle, checks your grip, checks the time, and if all pass → <Badge label="VALID ✓" color="var(--emerald-valid)" bg="rgba(0,230,118,0.1)" />
            </div>
          </Section>

          {/* ── REP COUNTERS ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>}
            title="Rep Counters"
          >
            <Row label="Validated Reps" tip="Reps that passed all 4 rules. This is your real score." accent="var(--emerald-valid)" value="✅ count" />
            <Row label="Rejected Attempts" tip="Reps that failed at least one rule. Check the reason in the Session Log below." accent="var(--crimson-fault)" value="❌ count" />
            <Row
              label="Acceptance Ratio"
              tip="Valid Reps ÷ Total Attempts × 100. 80% or higher = good consistent form. Below 60% = something is off (grip? ROM? too fast?)."
              accent="var(--cyan-primary)"
              value="Valid ÷ Total"
            />
          </Section>

          {/* ── MOVEMENT CHART ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/></svg>}
            title="Movement Trace (Chart)"
          >
            <Row
              label="Blue line"
              tip="Your wrist pitch angle in real time. Scrolls left every 100ms. A full bicep curl looks like a hill — rises then falls."
              value="Pitch angle"
            />
            <Row
              label="Green dashed line"
              tip="The minimum ROM threshold (35°). If your curl peak stays below this line the rep will be rejected for insufficient range of motion."
              accent="var(--emerald-valid)"
              value="Min ROM 35°"
            />
            <Row
              label="Current Excursion"
              tip="How far your wrist has moved from the starting angle right now, in degrees. This resets to 0 at the start of each new rep attempt."
              value="Live angle delta"
            />
            <Row
              label="Peak ROM (last rep)"
              tip="The maximum angle reached in the most recent rep. Shown in the chart header."
              value="Max of last rep"
            />
          </Section>

          {/* ── REP PHASES ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/></svg>}
            title="Rep Phase (Current Phase badge)"
            color="var(--amber-warning)"
          >
            {[
              ['IDLE', 'Glove is waiting. No movement detected.', 'var(--text-muted)', 'rgba(255,255,255,0.05)'],
              ['READY', 'Arm is in start position. Ready to begin.', '#60a5fa', 'rgba(96,165,250,0.1)'],
              ['CONCENTRIC', 'Lifting phase — curling the weight up.', 'var(--cyan-primary)', 'rgba(0,229,255,0.1)'],
              ['TRANSITION', 'Peak reached — about to reverse direction.', 'var(--amber-warning)', 'rgba(251,191,36,0.1)'],
              ['ECCENTRIC', 'Lowering phase — bringing weight back down.', '#c084fc', 'rgba(192,132,252,0.1)'],
              ['EVALUATING', 'Rep complete — glove is scoring it now.', '#fb923c', 'rgba(251,146,60,0.1)'],
              ['VALID_REP ✓', 'All rules passed! Rep counted.', 'var(--emerald-valid)', 'rgba(0,230,118,0.1)'],
              ['REJECTED_REP ✗', 'At least one rule failed. See reason in log.', 'var(--crimson-fault)', 'rgba(239,68,68,0.1)'],
            ].map(([state, desc, color, bg]) => (
              <div key={state} style={{ padding: '6px 0', borderBottom: '1px solid rgba(255,255,255,0.04)', display: 'flex', gap: '10px', alignItems: 'flex-start' }}>
                <Badge label={state} color={color} bg={bg} />
                <span style={{ fontSize: '11px', color: '#94a3b8', lineHeight: 1.5, paddingTop: '2px' }}>{desc}</span>
              </div>
            ))}
          </Section>

          {/* ── SENSOR PANEL ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><path d="M18 10h-1.26A8 8 0 1 0 9 20h9a5 5 0 0 0 0-10z"/></svg>}
            title="Sensor Panel (Live Readings)"
          >
            <Row
              label="FSR Grip %"
              tip="Force Sensitive Resistor. How hard you are squeezing relative to your firm grip (set during calibration). 0% = open hand, 100% = maximum squeeze recorded in calibration."
              accent="var(--cyan-primary)"
              value="0 – 100%"
            />
            <Row
              label="ADC value"
              tip="Raw 12-bit number from the grip sensor (0–4095). Higher = more pressure. This is the unprocessed signal before calibration scaling."
              value="0 – 4095"
            />
            <Row
              label="Voltage (V)"
              tip="Voltage across the FSR resistor divider (0–3.3V). Shows the analogue signal strength."
              value="0 – 3.3V"
            />
            <Row
              label="Pitch °"
              tip="Forward/backward tilt of your forearm. 0° = arm hanging down. ~80° = fully curled. This is the primary signal used for rep counting."
              value="Forearm angle"
            />
            <Row
              label="Roll °"
              tip="Side-to-side twist of your wrist. Ideally stays close to 0°. High roll during a curl means your wrist is rotating — can indicate poor form."
              value="Wrist twist"
            />
            <Row
              label="Accel X/Y/Z"
              tip="Acceleration in three axes in m/s². At rest you'll see ~9.8 m/s² on one axis (gravity). During a rep all three values change rapidly."
              value="m/s²"
            />
            <Row
              label="Gyro X/Y/Z"
              tip="Angular velocity in degrees per second. How fast the glove is rotating right now. During a curl expect 30–150 deg/s on the pitch axis."
              value="deg/s"
            />
            <Row
              label="Temp °C"
              tip="Temperature inside the MPU6050 chip (not ambient). Typically 30–40°C. Not used for rep logic, just a health indicator."
              value="Chip temp"
            />
            <Row
              label="STATIONARY / ACTIVE"
              tip="Stationary = the glove detects no movement for 1 second. The glove uses this to auto-correct gyro drift. During a rep this will show ACTIVE DYNAMICS."
              value="Motion state"
            />
          </Section>

          {/* ── RELIABILITY / HEALTH ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>}
            title="Sensor Reliability Centre"
            color="var(--emerald-valid)"
          >
            {[
              ['HEALTHY', 'var(--emerald-valid)', 'Both sensors reading normally. Rep counting is active.'],
              ['DEGRADED', 'var(--amber-warning)', 'Minor signal instability detected. Counting may continue but treat results with caution.'],
              ['SUSPECTED_FAULT', 'var(--crimson-fault)', 'Possible sensor problem. Rep counting is paused automatically.'],
              ['SENSOR_UNAVAILABLE', 'var(--crimson-fault)', 'Sensor not responding (IMU disconnected or FSR open circuit). Check wiring.'],
              ['RECOVERING', '#fb923c', 'A fault was detected but data is stabilising. Counting resumes after 1.5s of clean data.'],
              ['CALIBRATION_REQUIRED', '#c084fc', 'No personal calibration saved. Run the calibration wizard before working out.'],
            ].map(([state, color, desc]) => (
              <div key={state} style={{ padding: '6px 0', borderBottom: '1px solid rgba(255,255,255,0.04)', display: 'flex', gap: '10px', alignItems: 'flex-start' }}>
                <Badge label={state} color={color} bg={`${color}15`} />
                <span style={{ fontSize: '11px', color: '#94a3b8', lineHeight: 1.5, paddingTop: '2px' }}>{desc}</span>
              </div>
            ))}
            <Row
              label="Confidence %"
              tip="How certain the glove is about its health assessment. 80%+ = trustworthy. Below 50% = the health verdict itself is uncertain (usually during startup)."
              accent="var(--cyan-primary)"
              value="80%+ = good"
            />
          </Section>

          {/* ── SESSION REPORT ── */}
          <Section
            icon={<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/></svg>}
            title="Session Report (Log)"
          >
            <Row label="Duration (ms)" tip="How long that individual rep took from start to finish in milliseconds. 1000ms = 1 second. Aim for 1500–3000ms for controlled reps." value="per rep time" />
            <Row label="Peak ROM °" tip="The highest angle reached during that rep. The green dashed line on the chart is the 35° minimum. More is generally better." value="max angle hit" />
            <Row label="Avg Grip %" tip="Average grip percentage during the concentric (lifting) phase of that rep. Must be ≥40% to be valid." value="squeeze average" />
            <Row label="Export CSV / JSON" tip="Download the full session log. CSV is for Excel/Sheets. JSON includes calibration data too — useful for analysis." value="saves file" />
          </Section>

          <div style={{ fontSize: '10px', color: 'var(--text-dim)', textAlign: 'center', marginTop: '4px' }}>
            Hover any <span style={{ color: 'var(--cyan-primary)' }}>?</span> tooltip on a metric for instant help
          </div>
        </div>
      )}

      {/* ── Floating button ── */}
      <button
        onClick={() => setOpen((v) => !v)}
        title="Open Dashboard Guide"
        style={{
          width: '52px', height: '52px', borderRadius: '50%',
          background: open ? 'var(--cyan-primary)' : 'linear-gradient(135deg, #1a2744, #0e1830)',
          border: `2px solid ${open ? 'var(--cyan-primary)' : 'rgba(0,229,255,0.3)'}`,
          boxShadow: open ? '0 0 24px rgba(0,229,255,0.5)' : '0 8px 24px rgba(0,0,0,0.5)',
          color: open ? '#040814' : 'var(--cyan-primary)',
          fontSize: '22px', cursor: 'pointer', display: 'flex',
          alignItems: 'center', justifyContent: 'center',
          transition: 'all 0.25s ease',
        }}
      >
        {open ? '✕' : '?'}
      </button>
    </div>
  );
};
