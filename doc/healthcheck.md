@page healthcheck Health Check

The health check endpoint provides a simple way to verify that the MantisBase server is running and responsive. It is useful for monitoring tools, load balancers, and deployment scripts.

---

## Endpoint

```
GET /api/v1/health
```

---

## Response

A successful request returns:

```json
{
  "status": "OK"
}
```

The HTTP status code for a healthy response is `200 OK`.

---

## Example

**Request:**

```bash
curl http://localhost:7070/api/v1/health
```

**Response:**

```json
{
  "status": "OK"
}
```

---

## Error Handling

If the server encounters an error while processing the health check, it may return a `500 Internal Server Error` with an appropriate error message.

---

## Usage

- Verify server availability and responsiveness
- Integrate with monitoring systems (Prometheus, Nagios, etc.)
- Use in orchestration systems (Kubernetes liveness/readiness probes, Docker health checks)
- Load balancer health checks

**Example Docker health check:**

```dockerfile
HEALTHCHECK --interval=30s --timeout=3s \
  CMD curl -f http://localhost:7070/api/v1/health || exit 1
```

**Example Kubernetes liveness probe:**

```yaml
livenessProbe:
  httpGet:
    path: /api/v1/health
    port: 7070
  initialDelaySeconds: 30
  periodSeconds: 10
```
