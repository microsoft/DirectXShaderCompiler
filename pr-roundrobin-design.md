# PR Round-Robin Assignee Service — Design Plan

## Overview

A GitHub App backed by an Azure Function that automatically assigns a team member to every new pull request across multiple watched repositories using a global round-robin rotation. The service is **fully external** — no workflow files or code changes are required in any watched repo.

---

## Architecture

```
┌─────────────────────┐
│  Watched Repos      │  (no code changes needed)
│  - org/repo-a       │
│  - org/repo-b       │
│  - org/repo-c       │
└────────┬────────────┘
         │  pull_request.opened webhook
         ▼
┌─────────────────────┐
│  GitHub App          │
│  (installed on repos │
│   or org-wide)       │
└────────┬────────────┘
         │  POST webhook
         ▼
┌─────────────────────┐      ┌──────────────────────┐
│  Azure Function      │◄────►│  Azure Table Storage  │
│  (TypeScript, v4)    │      │  - rotation queue      │
│                      │      │  - watched repos list  │
└──────────────────────┘      └──────────────────────┘
```

### Components

| Component | Purpose |
|-----------|---------|
| **GitHub App** | Installed on target repos/org. Receives `pull_request` webhooks. Provides authentication via app installation tokens. |
| **Azure Function** | HTTP-triggered function that processes webhooks, reads/updates rotation state, and calls GitHub API to assign PRs. |
| **Azure Table Storage** | Stores the rotation queue (ordered list of team members) and the list of watched repos. Comes free with the Azure Functions storage account. |

---

## Round-Robin Mechanism — Rotating List

Instead of a simple counter, the rotation is maintained as an **ordered list**. The person at the front of the list is assigned next, then moved to the back.

### Example

Starting state:
```json
["alice", "bob", "charlie"]
```

1. New PR arrives → **alice** is assigned → state becomes `["bob", "charlie", "alice"]`
2. New PR arrives → **bob** is assigned → state becomes `["charlie", "alice", "bob"]`
3. New PR arrives → **charlie** is assigned → state becomes `["alice", "bob", "charlie"]`

### Benefits of this approach
- **Stable under team changes.** Adding or removing a member doesn't shift who's "next." Just insert/remove from the list.
- **Manually adjustable.** An admin can reorder the list at any time (e.g., move someone to the back if they just got assigned a heavy PR).
- **Transparent.** The current state is always human-readable — whoever is first in the list is next.

### Removing someone temporarily
To mark someone unavailable, simply remove them from the list. When they return, insert them at the desired position (e.g., at the front to get the next assignment, or at the back).

---

## Data Model (Azure Table Storage)

Azure Table Storage is used because it's included with the Azure Functions storage account (no additional cost or provisioning).

### Table: `RotationQueue`

Single row that holds the ordered list.

| PartitionKey | RowKey | Queue (JSON string) |
|---|---|---|
| `global` | `rotation` | `["alice", "bob", "charlie"]` |

### Table: `WatchedRepos`

| PartitionKey | RowKey | Notes |
|---|---|---|
| `repos` | `microsoft/DirectXShaderCompiler` | |
| `repos` | `microsoft/DirectXTK` | |
| `repos` | `other-org/other-repo` | |

### Table: `AssignmentLog` (optional, for debugging)

| PartitionKey | RowKey | Repo | PR | Assignee | Timestamp |
|---|---|---|---|---|---|
| `2026-03` | `{guid}` | `microsoft/repo-a` | `#1234` | `alice` | `2026-03-12T18:00:00Z` |

---

## Azure Function — Webhook Handler

### Runtime
- **Language:** TypeScript (Node.js runtime)
- **Azure Functions v4** programming model
- **Trigger:** HTTP (webhook endpoint)

TypeScript is recommended over Python for Azure Functions because:
- First-class Azure Functions v4 support
- Strong typing catches webhook payload issues at compile time
- Excellent GitHub API libraries (`@octokit/rest`, `@octokit/webhooks`)
- Fast cold starts on Node.js runtime

### Endpoint

```
POST /api/github-webhook
```

### Logic (pseudocode)

```typescript
async function handleWebhook(request):
    // 1. Verify webhook signature (using GitHub App webhook secret)
    verifySignature(request.headers["x-hub-signature-256"], request.body)

    // 2. Parse event
    event = request.headers["x-github-event"]
    payload = request.body

    // 3. Only handle pull_request.opened
    if event !== "pull_request" or payload.action !== "opened":
        return 200  // ignore

    // 4. Check if repo is watched
    repo = payload.repository.full_name
    if not isWatchedRepo(repo):
        return 200  // ignore

    // 5. Check if PR already has assignees
    if payload.pull_request.assignees.length > 0:
        return 200  // already assigned, skip

    // 6. Get next person from rotation queue
    queue = readQueue()          // ["alice", "bob", "charlie"]
    assignee = queue.shift()     // "alice"
    queue.push(assignee)         // ["bob", "charlie", "alice"]
    writeQueue(queue)            // persist

    // 7. Assign to PR via GitHub API
    octokit = getInstallationClient(payload.installation.id)
    await octokit.issues.addAssignees({
        owner: payload.repository.owner.login,
        repo: payload.repository.name,
        issue_number: payload.pull_request.number,
        assignees: [assignee]
    })

    return 200
```

### Concurrency

Azure Table Storage supports ETags for optimistic concurrency. The function should:
1. Read the queue row (including its ETag)
2. Modify the queue
3. Write back with `If-Match: <etag>`
4. If a `412 Precondition Failed` occurs (another instance modified the row), retry from step 1

This handles the (rare) case of two PRs arriving at the exact same moment.

---

## GitHub App Setup

### Required Permissions
| Permission | Access | Reason |
|---|---|---|
| Pull requests | Read & Write | To read assignees and set new ones |
| Metadata | Read | Required by GitHub |

### Webhook Events
| Event | Reason |
|---|---|
| `pull_request` | Fires on PR opened |

### Installation
Install the app on the **organization** (preferred) or on individual repositories. The app will only act on repos listed in the `WatchedRepos` table, so org-wide installation is safe and simplifies adding new repos.

---

## Deployment

### Prerequisites
- Azure subscription
- GitHub organization admin access (to create and install the App)

### Steps

1. **Create the GitHub App**
   - Go to org settings → Developer settings → GitHub Apps → New
   - Set webhook URL to the Azure Function endpoint
   - Set permissions and events as above
   - Generate and download the private key
   - Note the App ID and webhook secret

2. **Create Azure Resources**
   ```bash
   # Resource group
   az group create --name rg-pr-roundrobin --location eastus2

   # Storage account (also used by Azure Functions runtime)
   az storage account create \
     --name stprroundrobin \
     --resource-group rg-pr-roundrobin \
     --sku Standard_LRS

   # Function App (Node.js 20 LTS)
   az functionapp create \
     --name fn-pr-roundrobin \
     --resource-group rg-pr-roundrobin \
     --storage-account stprroundrobin \
     --runtime node \
     --runtime-version 20 \
     --functions-version 4 \
     --consumption-plan-location eastus2
   ```

3. **Configure App Settings**
   ```bash
   az functionapp config appsettings set \
     --name fn-pr-roundrobin \
     --resource-group rg-pr-roundrobin \
     --settings \
       GITHUB_APP_ID="<app-id>" \
       GITHUB_WEBHOOK_SECRET="<webhook-secret>" \
       GITHUB_PRIVATE_KEY="<base64-encoded-pem>"
   ```

4. **Initialize Storage Tables**
   - Create tables: `RotationQueue`, `WatchedRepos`, `AssignmentLog`
   - Seed the `RotationQueue` with the initial team list
   - Add repos to `WatchedRepos`

5. **Deploy the Function**
   ```bash
   func azure functionapp publish fn-pr-roundrobin
   ```

6. **Install the GitHub App** on the target organization/repos

---

## Project Structure

```
pr-roundrobin/
├── src/
│   └── functions/
│       └── github-webhook.ts      # Main webhook handler
├── src/
│   ├── github.ts                  # GitHub App auth & API calls
│   ├── storage.ts                 # Azure Table Storage operations
│   └── types.ts                   # TypeScript interfaces
├── host.json                      # Azure Functions host config
├── package.json
├── tsconfig.json
└── README.md
```

---

## Admin Operations

These can be done via Azure Storage Explorer, Azure Portal, or a simple CLI/script:

| Operation | How |
|---|---|
| **View current rotation** | Read the `RotationQueue` row |
| **Remove a team member** | Edit the queue JSON, remove their name |
| **Add a team member** | Edit the queue JSON, insert at desired position |
| **Reorder the queue** | Edit the queue JSON directly |
| **Add a watched repo** | Insert a row into `WatchedRepos` |
| **Remove a watched repo** | Delete the row from `WatchedRepos` |
| **View assignment history** | Query `AssignmentLog` table |

For convenience, a future v2 could add admin endpoints to the Azure Function itself (protected by a function key):
- `GET /api/admin/queue` — view current rotation
- `PUT /api/admin/queue` — update rotation
- `GET /api/admin/repos` — list watched repos
- `POST /api/admin/repos` — add a watched repo

---

## Cost Estimate

| Resource | Expected Cost |
|---|---|
| Azure Functions (Consumption plan) | Free tier covers ~1M executions/month |
| Azure Table Storage | Negligible (pennies/month for this volume) |
| GitHub App | Free |

**Total estimated cost: ~$0/month** for typical PR volumes.

---

## Edge Cases & Considerations

| Scenario | Behavior |
|---|---|
| PR opened with assignee already set | Skip (no-op) |
| PR opened on unwatched repo | Skip (no-op) |
| Rotation queue is empty | Log error, skip assignment |
| PR author is next in rotation | Assign anyway (v1 simplicity) |
| Two PRs arrive simultaneously | Optimistic concurrency retry handles this |
| GitHub App not installed on repo | Webhook won't fire; no action needed |
| Webhook delivery fails | GitHub retries webhook delivery automatically |

---

## Future Enhancements (v2+)

- **Skip PR author:** Don't assign the PR to the person who opened it; assign to next in queue instead.
- **Admin API endpoints:** REST endpoints on the Azure Function for queue/repo management.
- **Slack/Teams notifications:** Notify the assigned person via chat.
- **Availability/vacation support:** Integrate with a calendar or manual availability flag.
- **Dashboard:** Simple web UI showing current rotation, recent assignments, and watched repos.
- **Multiple teams:** Support different rotation queues for different repo groups.
