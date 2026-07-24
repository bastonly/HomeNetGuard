# Client Activation & Checkin API Documentation

**Base URL**: `/api/v1`

## 1. Device Activation
Used when the app first starts or when the user enters a new license code.

- **Endpoint**: `POST /activate`
- **Headers**:
    - `X-Customer-Token`: `<customer_api_token>` (Required)
- **Request Body** (`application/json`):
    ```json
    {
        "license_id": "LICENSE-CODE-1234",
        "device_fingerprint": "unique_hardware_id_hash",
        "model": "iPhone 13",
        "os_version": "iOS 15.0",
        "app_version": "1.0.1",
        "meta_info": {
            "hostname": "John's iPhone",
            "language": "en-US"
        }
    }
    ```

- **Response** (`application/json`):
    ```json
    {
        "status": "ok",  // "ok" or "fail"
        "message": "Device activated",
        "license_id": "LIC-202X-XXXX",
        "license_status": 1, 
        "server_time": "2026-02-10T22:30:00.000000",
        "lastest_api_version": "1.0.0",
        "expired_date": "2027-02-10T22:30:00"  // Optional, if license has expiration
    }
    ```

### Error Response Example
```json
{
    "status": "fail",
    "message": "Internal Error: ...", // or "Invalid activation code", "License expired", etc.
    "server_time": "..."
}
```

---

## 2. Device Checkin
Used periodically (e.g., app launch) to verify license validity and update "last seen" status.

- **Endpoint**: `POST /checkin`
- **Headers**:
    - `X-Customer-Token`: `<customer_api_key>` (Optional but recommended)
- **Request Body** (`application/json`):
    ```json
    {
        "license_id": "LIC-202X-XXXX",
        "device_fingerprint": "unique_hardware_id_hash",
        "meta_info": {} 
    }
    ```

- **Response** (`application/json`):
    ```json
    {
        "status": "ok",
        "message": null,
        "license_status": 1,
        "token_issued": 1,
        "server_time": "2026-02-10T22:35:00.000000"
    }
    ```

---

## Reference Manuals

### License Status Codes (`license_status`)
| Code | Status    | Description |
| ---- | --------- | ----------- |
| 0    | PENDING   | License generated but not yet activated. |
| 1    | ISSUED    | Active and valid. |
| 2    | EXPIRED   | License time has finished. |
| 3    | REVOKED   | Manually revoked by admin. |
| 4    | SUSPENDED | Temporarily suspended by admin. |

### Device Status Codes (Internal)
| Code | Status  | Description |
| ---- | ------- | ----------- |
| 0    | FAIL    | Activation failed or expired. |
| 1    | ACTIVE  | Currently bound and active. |
| 2    | UNBOUND | Device unbound (reset) or archived. |
| 3    | REVOKED | Device manually blocked. |
