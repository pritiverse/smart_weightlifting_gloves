import React from 'react';
import { HistoricalRepItem } from '../types';

interface SessionReportProps {
  history: HistoricalRepItem[];
  onExportCSV: () => void;
  onExportJSON: () => void;
}

export const SessionReport: React.FC<SessionReportProps> = ({ history, onExportCSV, onExportJSON }) => {
  return (
    <div className="dashboard-card grid-col-6">
      <div className="card-header">
        <h2 className="card-title">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="var(--cyan-primary)" strokeWidth="2.5">
            <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
            <polyline points="14 2 14 8 20 8" />
            <line x1="16" y1="13" x2="8" y2="13" />
            <line x1="16" y1="17" x2="8" y2="17" />
            <polyline points="10 9 9 9 8 9" />
          </svg>
          Session Repetition Log & Data Export
        </h2>
        <div style={{ display: 'flex', gap: '8px' }}>
          <button onClick={onExportCSV} className="btn-secondary" style={{ padding: '4px 10px', fontSize: '11px' }}>
            Export CSV
          </button>
          <button onClick={onExportJSON} className="btn-secondary" style={{ padding: '4px 10px', fontSize: '11px' }}>
            Export JSON
          </button>
        </div>
      </div>

      <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
        {history.length === 0 ? (
          <div style={{ textAlign: 'center', padding: '30px', color: 'var(--text-dim)', fontSize: '13px' }}>
            No repetitions logged yet. Perform calibrated repetitions to generate session analytics.
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '12px', textAlign: 'left' }}>
            <thead>
              <tr style={{ color: 'var(--text-dim)', borderBottom: '1px solid var(--border-subtle)' }}>
                <th style={{ padding: '6px 8px' }}>REP #</th>
                <th style={{ padding: '6px 8px' }}>STATUS</th>
                <th style={{ padding: '6px 8px' }}>ROM</th>
                <th style={{ padding: '6px 8px' }}>DURATION</th>
                <th style={{ padding: '6px 8px' }}>GRIP</th>
                <th style={{ padding: '6px 8px' }}>EXPLANATION</th>
              </tr>
            </thead>
            <tbody>
              {history.map((item) => (
                <tr key={item.id} style={{ borderBottom: '1px solid var(--border-subtle)', fontFamily: 'var(--font-mono)' }}>
                  <td style={{ padding: '8px', color: '#fff', fontWeight: 700 }}>#{item.repNumber}</td>
                  <td style={{ padding: '8px' }}>
                    <span
                      style={{
                        padding: '2px 6px',
                        borderRadius: '4px',
                        fontSize: '10px',
                        fontWeight: 700,
                        background: item.isValid ? 'rgba(0, 230, 118, 0.15)' : 'rgba(255, 23, 68, 0.15)',
                        color: item.isValid ? 'var(--emerald-valid)' : 'var(--crimson-fault)',
                      }}
                    >
                      {item.isValid ? 'VALID' : 'REJECTED'}
                    </span>
                  </td>
                  <td style={{ padding: '8px', color: '#fff' }}>{item.peakRomDeg}°</td>
                  <td style={{ padding: '8px', color: 'var(--text-muted)' }}>{item.durationMs}ms</td>
                  <td style={{ padding: '8px', color: 'var(--cyan-primary)' }}>{item.avgGripPct}%</td>
                  <td style={{ padding: '8px', color: 'var(--text-dim)', fontSize: '11px', fontFamily: 'var(--font-body)' }}>
                    {item.rejectionReason}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
};
