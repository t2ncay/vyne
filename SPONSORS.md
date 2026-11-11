# Sponsors

Vyne is an independent, open-source numerical language and compiler project.
Sponsorship pays for the boring-but-essential infrastructure that keeps the
project healthy: continuous integration, GPU and embedded test hardware,
documentation, security review, release tooling, and focused maintenance time.

This file is the source of truth for sponsor recognition. It also documents
how sponsors are added, how funds are used, and the rules that keep the
project technically independent.

## Current sponsors

| Sponsor | Tier | Since | Recognition |
| --- | --- | --- | --- |
| **Zeka Shamilli** | Founding Sponsor | 2026 | Name |

Founding Sponsor is a historical designation for the first supporters who made
the project's long-term infrastructure viable. It is not a purchasable tier.

## Why sponsor Vyne

Vyne is building a practical bridge between high-level numerical code and
explicit, predictable resource management:

- **A real language and compiler**, not a wrapper around a Python runtime.
- **Explicit memory and device regions** (`@device<cuda>`, pools, slabs,
  rings, streams) that make performance and lifetime visible.
- **Numerical libraries** for convolution, linear algebra, FFTs, sparse
  operators, optimization, statistics, and image/video processing.
- **Embedded and edge targets**, including ESP32, where a small runtime and
  predictable allocation matter more than raw throughput.
- **Training and inference paths** that can stay on the device from forward
  pass through backward pass.
- **Open licenses**: the compiler is AGPL-3.0-only; runtime modules are MIT.

Sponsorship lets maintainers spend less time on infrastructure and more time
on correctness, documentation, benchmarks, and the long tail of platform
support.

## Sponsorship tiers

Amounts are suggested monthly contributions. Custom arrangements, one-time
grants, hardware donations, CI credits, and in-kind support are welcome.

| Tier | Suggested | Recognition | Best for |
| --- | --- | --- | --- |
| **Community Supporter** | $5+/month | Name in this file (optional) | Individuals who want to say thanks |
| **Bronze** | $100/month | Name and link | Small teams and early users |
| **Silver** | $500/month | Name, link, and short description | Companies shipping on Vyne |
| **Gold** | $1,000/month | Logo, link, and description | Organisations with production use |
| **Platinum** | $5,000/month | Logo, link, description, and release-note acknowledgement | Strategic infrastructure partners |
| **Strategic Partner** | Custom | Custom recognition and quarterly roadmap briefing | Hardware vendors, cloud providers, and research groups |

Recognition is opt-in. Sponsors may request name-only, logo-only, or fully
anonymous recognition. Anonymous sponsors are still recorded privately for
accounting and continuity.

### What sponsorship does not buy

Sponsorship is support for the project, not a product contract. It does not
grant:

- control over the roadmap, API design, or release schedule;
- priority over security or correctness work;
- the right to relicense Vyne or its dependencies;
- endorsement of a sponsor's product by the project;
- access to private user data or unreleased security reports.

Commercial support, custom development, and SLA-backed engagements are
separate agreements and must be documented in writing.

## How funds are used

Sponsorship is allocated in roughly this order:

1. **Continuous integration and release infrastructure** — runners, storage,
   artifact hosting, and code-signing certificates.
2. **Hardware coverage** — GPUs, embedded boards, and CI machines for
   platforms that cannot be exercised in software.
3. **Security and dependency maintenance** — audits, fuzzing, and upgrades.
4. **Documentation and examples** — tutorials, API references, and runnable
   projects.
5. **Maintainer time** — focused work on bugs, reviews, and releases.
6. **Community programs** — bounties, contributor support, and conference
   travel where it directly benefits the project.

A short annual summary of sponsorship income and major expenses will be
published when the project has enough sponsorship activity to make the
summary meaningful. Individual sponsor amounts are not disclosed without
permission.

## Adding a new sponsor

Maintainers should follow this checklist so recognition stays accurate and
consistent.

1. Confirm the sponsor's display name, tier, start date, link, and preferred
   recognition form (name, logo, description, or anonymous).
2. Confirm that the sponsor has approved the exact wording and logo asset.
3. Add or update the sponsor in the **Current sponsors** table.
4. If a logo is used, place it under `docs/assets/sponsors/` using a
   lower-case, hyphenated slug, and reference it from the table.
5. Keep the table ordered by tier, then by start date, then alphabetically by
   display name.
6. Never claim a sponsorship that has not been confirmed in writing.
7. Never add a sponsor's logo or name to a release, website, or package
   without the sponsor's approval.
8. Record the sponsorship start date and renewal date in the private finance
   ledger.

### Sponsor record template

Use this block when collecting sponsor details before updating the table:

```yaml
sponsor:
  display_name: "Example Org"
  tier: "Gold" # Community | Bronze | Silver | Gold | Platinum | Strategic
  since: "2026-11" # YYYY-MM
  url: "https://example.com"
  logo: "docs/assets/sponsors/example-org.svg" # optional
  description: "Short, sponsor-approved description." # optional
  recognition: "logo" # name | logo | anonymous
  approved_by: "maintainer-handle"
  approved_on: "2026-11-12"
  renewal: "2027-11" # optional
```

### Removing or changing a sponsor

- Downgrades take effect at the next renewal date unless both sides agree
  otherwise.
- Removals requested by the sponsor take effect immediately.
- Recognition may be removed immediately if a sponsor violates the project's
  Code of Conduct or misrepresents the relationship.
- Historical recognition may remain in archived release notes even after the
  sponsor stops supporting the project.

## Sponsorship workflow

1. Open an issue at <https://github.com/t2ncay/vyne/issues> with a title
   beginning `[Sponsorship]`.
2. Include the desired tier, recognition preference, and any invoicing or
   procurement requirements.
3. A maintainer will confirm the arrangement and the exact recognition text.
4. The sponsor is added to this file after confirmation.
5. Renewal is reviewed before the anniversary date; no automatic renewal is
   assumed without confirmation.

For sponsorship questions that should not be public, contact the maintainers
through the repository's private security or maintainer channel rather than
opening a public issue.

## Governance and independence

Vyne is maintained as an independent open-source project. Sponsors are valued
supporters, not owners of the technical direction. Roadmap decisions remain
with the maintainers and are made on technical merit, user impact, and
long-term maintainability.

To keep the boundary clear:

- Sponsorship does not create a seat on the maintainer team.
- Sponsor requests are treated like any other user request.
- Security fixes are never delayed for sponsor marketing.
- The project will disclose any arrangement that could reasonably be seen as
  a conflict of interest.

## Recognition policy

- Names and logos are displayed only with permission.
- Descriptive text is limited to what the sponsor has approved.
- The project does not imply that a sponsor uses, recommends, or certifies
  Vyne unless the sponsor has explicitly approved that claim.
- Trademarks remain the property of their owners.
- Recognition in this file does not grant any trademark or patent license
  beyond the project's open-source licenses.

## Document history

| Date | Change |
| --- | --- |
| 2026-11-12 | Initial sponsor policy and Founding Sponsor recognition. |

Thank you to every sponsor, contributor, and user who makes Vyne possible.
