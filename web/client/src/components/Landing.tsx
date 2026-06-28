// Landing page — hero, feature highlights, and a CTA into the app.

export function Landing({ onEnter }: { onEnter: () => void }) {
  return (
    <div style={{ minHeight: '100%', display: 'flex', flexDirection: 'column' }}>
      <header className="app-header">
        <div className="app-title">
          VDD <span className="accent">Audit</span> &amp; Verification
        </div>
        <button className="btn-accent" style={{ marginLeft: 'auto' }} onClick={onEnter}>
          Launch App →
        </button>
      </header>

      <section style={{ maxWidth: 1000, margin: '0 auto', padding: '64px 24px 28px', textAlign: 'center' }}>
        <div
          style={{
            display: 'inline-block',
            padding: '6px 14px',
            borderRadius: 999,
            border: '1px solid var(--panel-border)',
            background: 'rgba(99,102,241,0.1)',
            color: 'var(--indigo-bright)',
            fontWeight: 700,
            fontSize: 12,
            letterSpacing: 1,
            marginBottom: 20,
          }}
        >
          AI-POWERED INTEGRITY AUDITS
        </div>
        <h1 style={{ fontSize: 46, lineHeight: 1.1, margin: '0 0 16px' }}>
          Verify your Version Description Documents
          <br />
          <span style={{ color: 'var(--indigo-bright)' }}>against the real files.</span>
        </h1>
        <p className="muted" style={{ fontSize: 18, maxWidth: 680, margin: '0 auto 28px' }}>
          Import a VDD, let the LLM extract every configuration item, then verify each one against your local and
          network file shares with MD5, SHA-1, and CRC-32 checksums — all in your browser.
        </p>
        <div className="row" style={{ justifyContent: 'center' }}>
          <button className="btn-primary" onClick={onEnter}>
            Get Started
          </button>
          <a
            href="https://github.com/bala9066/CS"
            target="_blank"
            rel="noreferrer"
            className="btn-ghost"
            style={{ textDecoration: 'none', display: 'inline-flex', alignItems: 'center' }}
          >
            View Source
          </a>
        </div>
        <p className="muted" style={{ marginTop: 14, fontSize: 13 }}>
          A native Qt 5.14 desktop build is also available (<span className="mono">release/ChecksumHasher.exe</span>).
        </p>
      </section>

      <section style={{ maxWidth: 1100, margin: '0 auto', padding: '20px 24px 60px' }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 18 }}>
          <Feature
            icon="📄"
            title="VDD Audit"
            body="Import an ODT VDD, extract CI references / versions / checksums via the LLM, resolve files from your config's Document Links, and verify integrity. Export results to CSV."
          />
          <Feature
            icon="#"
            title="Bulk Checksum"
            body="Point at any list of files — local or UNC network shares — and compute MD5, SHA-1, and CRC-32 in one pass, with per-file results."
          />
          <Feature
            icon="🔍"
            title="Document Review"
            body="Extract an ODT's text and run an LLM quality + CI-consistency review to catch issues before sign-off."
          />
        </div>
      </section>

      <footer style={{ marginTop: 'auto', padding: '18px 24px', borderTop: '1px solid var(--panel-border)', textAlign: 'center' }} className="muted">
        VDD Automated Audit &amp; Verification System · Web Edition
      </footer>
    </div>
  );
}

function Feature({ icon, title, body }: { icon: string; title: string; body: string }) {
  return (
    <div className="panel" style={{ padding: 22 }}>
      <div style={{ fontSize: 28, marginBottom: 10 }}>{icon}</div>
      <h3 style={{ margin: '0 0 8px' }}>{title}</h3>
      <p className="muted" style={{ margin: 0, lineHeight: 1.5 }}>
        {body}
      </p>
    </div>
  );
}
