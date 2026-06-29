import './landing.css';

export function Landing({ onEnter }: { onEnter: () => void }) {
  return (
    <div className="l-page">
      <div className="l-grid" />
      <div className="l-scan" />

      <header className="l-header">
        <div className="l-logo">
          VDD<span className="l-logo-sep">·</span>AUDIT
          <span className="l-logo-dot" />
        </div>
        <div className="l-header-right">
          <span className="l-online-tag">SYSTEM ONLINE</span>
          <button className="l-launch-btn" onClick={onEnter}>
            Launch →
          </button>
        </div>
      </header>

      <section className="l-hero">
        <div className="l-glow-hero" />

        <div className="l-system-tag">
          Automated Integrity Verification<span className="l-cursor">_</span>
        </div>

        <h1 className="l-h1">
          <span className="l-h1-line1">Verify</span>
          <span className="l-h1-line2">With Certainty</span>
          <span className="l-h1-ghost">No Assumptions. No Shortcuts.</span>
        </h1>

        <p className="l-sub">
          Import a Version Description Document. Let the AI extract every configuration item.
          Verify each file against your local drives and network shares with MD5, SHA&#8209;1,
          and CRC&#8209;32 checksums — all from your browser.
        </p>

        <div className="l-cta">
          <button className="l-btn-primary" onClick={onEnter}>
            Get Started
          </button>
          <a
            href="https://github.com/bala9066/CS"
            target="_blank"
            rel="noreferrer"
            className="l-btn-ghost"
          >
            ↗ View Source
          </a>
        </div>

        <div className="l-pills">
          <span className="l-pill">MD5</span>
          <span className="l-pill">SHA-1</span>
          <span className="l-pill">CRC-32</span>
          <span className="l-pill">LLM Extraction</span>
          <span className="l-pill">UNC Shares</span>
          <span className="l-pill">ODT / CSV / XLSX</span>
        </div>
      </section>

      <div className="l-section-header">
        <div className="l-section-label">Capabilities</div>
        <hr className="l-section-rule" />
      </div>

      <section className="l-features">
        <div className="l-cards">
          <div
            className="l-card"
            style={{ '--c-accent': '#06b6d4', animationDelay: '0.1s' } as React.CSSProperties}
          >
            <div className="l-card-num">01</div>
            <h3 className="l-card-title">VDD Audit</h3>
            <p className="l-card-body">
              Import an ODT document. The LLM extracts every CI reference, version, and checksum.
              Resolve files from Document Links and verify integrity in one pass. Export to CSV.
            </p>
            <div className="l-card-tag">ODT → LLM → Verify → CSV</div>
          </div>

          <div
            className="l-card"
            style={{ '--c-accent': '#6366f1', animationDelay: '0.2s' } as React.CSSProperties}
          >
            <div className="l-card-num">02</div>
            <h3 className="l-card-title">Bulk Checksum</h3>
            <p className="l-card-body">
              Paste any list of file paths — local drives or UNC network shares — and compute
              MD5, SHA&#8209;1, and CRC&#8209;32 for each in a single batch operation.
            </p>
            <div className="l-card-tag" style={{ color: '#6366f1' }}>Local · \\Network · Share</div>
          </div>

          <div
            className="l-card"
            style={{ '--c-accent': '#a855f7', animationDelay: '0.3s' } as React.CSSProperties}
          >
            <div className="l-card-num">03</div>
            <h3 className="l-card-title">Document Review</h3>
            <p className="l-card-body">
              Extract structured text from any ODT and run an LLM quality + CI&#8209;consistency
              review to catch formatting issues and gaps before sign&#8209;off.
            </p>
            <div className="l-card-tag" style={{ color: '#a855f7' }}>Extract · Analyze · Report</div>
          </div>
        </div>
      </section>

      <footer className="l-footer">
        <div className="l-footer-text">
          VDD Audit System · Web Edition · Qt&nbsp;5.14 desktop also available
        </div>
        <div className="l-footer-status">
          <span className="l-status-dot" />
          Backend Connected
        </div>
      </footer>
    </div>
  );
}
