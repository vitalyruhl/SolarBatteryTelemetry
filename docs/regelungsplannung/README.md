# SIL Control Planning - Round 5

Current baseline: [Round 5 decisions and initial constants](round-5-decisions-and-defaults.md). Round 5 takes precedence over older proposals; no device settings have been changed.

Owner-approved first design; planning closed on 2026-10-17. These English documents define the implementation baseline, not deployed behavior or operational/HITL acceptance. The current Round 5 baseline supersedes the historical Round 4 proposals and review. The folder is in the SolarBatteryTelemetry repository at the owner's request, while the control design concerns SIL.

## Main flow

[Main discussion flow](<SIL - Ablaufdiagramm für die gemeinsame Diskussion.md>)

## Subprograms

- [Measurement freshness and plausibility](01-messwerte.md)
- [Fan demand](02-luefter.md)
- [Directional protection](03-schutzpruefung.md)
- [Protection response](04-schutzreaktion.md)
- [Energy planning](05-energieplanung.md)
- [Energy budget](06-energiebudget.md)
- [Grid PID](07-netz-pid.md)
- [Setpoint sending and feedback](08-senden-ruecklesen.md)
- [Settings and callbacks](09-einstellungen-callback.md)
- [Round 4 decisions (historical)](round-4-decisions.md)
- [Round 5 decisions and initial constants (current)](round-5-decisions-and-defaults.md)
- [Independent Round 4 review (historical findings)](round-4-independent-review.md)


## Final accepted refinements

- Keep the overall MOS write gate and add a separate Hand/Auto selector.
- Expose separate reserve OFF/ON settings, initially 2%/10%.
- Expose the battery power direction band as a required setting (initial default 50 W).
- Reuse RS485 Publish Period (s), currently 2 seconds per owner; no extra command-change threshold.
- Use separate economic ramp-up and ramp-down constants, initially 50 W/s each, optionally configurable later.
- Bypass economic ramps for sudden load reductions and required reserve/protection/OFF/negative-price restrictions.
- Keep source data qualification, hardware behavior, and CM development-version HITL checks as implementation acceptance gates.

## Export to PDF

The source documents retain their existing print frontmatter, including the portrait layout in `05-energieplanung.md`. In VS Code, use Markdown Preview Enhanced (`shd101wyy.markdown-preview-enhanced`), open its side preview, then select **Chrome (Puppeteer) -> PDF**. Review page breaks before printing and re-export after Markdown changes. See the [extension and Mermaid support](https://github.com/shd101wyy/vscode-markdown-preview-enhanced) and [PDF export guide](https://github.com/shd101wyy/markdown-preview-enhanced/blob/master/docs/puppeteer.md).
