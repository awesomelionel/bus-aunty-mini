# Bus Timer API

A Cloudflare Worker API that provides real-time bus arrival information for multiple bus stops.

## Overview

This API fetches bus arrival timing data from a Supabase database and returns formatted information for up to 4 bus stops in a single request.

## API Endpoints

### Get Bus Arrivals
```
GET /api/v1/BusArrival
```

### Query Parameters

- `BusStopCode`: Bus stop code (4-6 digits)
  - Can specify multiple bus stops (maximum 4)
  - Example for single bus stop: `?BusStopCode=67379`
  - Example for multiple bus stops: `?BusStopCode=67379&BusStopCode=67371`

### Response Format

The API returns data in a standardized format for all requests:
```json
{
  "busStops": [
    {
      "BusStopCode": 67379,
      "Services": [
        {
          "ServiceNo": "123",
          "NextBus": {
            "EstimatedArrival": "2024-03-20T20:34:56+08:00",
            "Load": "SEA",
            "Feature": "WAB",
            "Type": "SD"
          },
          "NextBus2": {
            "EstimatedArrival": "2024-03-20T20:44:56+08:00",
            "Load": "SDA",
            "Feature": "WAB",
            "Type": "SD"
          },
          "NextBus3": {
            "EstimatedArrival": "2024-03-20T20:54:56+08:00",
            "Load": "LSD",
            "Feature": "WAB",
            "Type": "SD"
          }
        }
      ],
      "UpdatedAt": "2024-03-20T20:34:56+08:00"
    }
  ]
}
```

> **Note (verified against the live API on 2026-09-02):** `BusStopCode` is returned as a JSON **number**, not a string, despite the query parameter being passed as text. `EstimatedArrival`/`UpdatedAt` are ISO 8601 timestamps with an explicit **`+08:00` offset** (Singapore time), not a `Z`-suffixed UTC timestamp — clients must account for the offset when converting to UTC/epoch. This corrects an earlier version of this doc that showed both fields differently.

#### Field Descriptions

- `BusStopCode`: The unique identifier for the bus stop, as a JSON number
- `Services`: Array of bus services at this stop
  - `ServiceNo`: Bus service number
  - `NextBus`, `NextBus2`, `NextBus3`: Information about the next three arriving buses
    - `EstimatedArrival`: Expected arrival time in ISO 8601 format with a `+08:00` timezone offset (not UTC/`Z`)
    - `Load`: Bus loading status (SEA: Seats Available, SDA: Standing Available, LSD: Limited Standing)
    - `Feature`: Bus features (WAB: Wheelchair Accessible)
    - `Type`: Bus type (SD: Single Deck, DD: Double Deck, BD: Bendy)
- `UpdatedAt`: Last update time of the data in ISO 8601 format with a `+08:00` timezone offset

### Status Codes

- `200`: Successful request
- `404`: 
  - No bus stops provided
  - Invalid number of bus stops (must be 1-4)
  - Invalid bus stop code format
  - No data found for provided bus stops
- `500`: Internal server error

## Development

### Prerequisites
- Node.js (v18 or later recommended)
- npm
- Cloudflare Workers account
- Supabase account and project

### Environment Variables

Required environment variables:
```
SUPABASE_URL=your_supabase_url
SUPABASE_ANON_KEY=your_supabase_anon_key
```

You can set these using:
1. `.dev.vars` file for local development
2. Cloudflare dashboard for production
3. `wrangler secret` command

### Installation
```bash
# Install dependencies
npm install

# Generate Cloudflare Worker types
npm run cf-typegen
```

### Running Locally
```bash
npm run dev
```

### Testing
```bash
npm test
```

### Deployment
```bash
npm run deploy
```

## Project Structure
```
├── src/
│   ├── routes/
│   │   └── v1/
│   │       └── bus-timing.ts    # V1 bus timing endpoint
│   ├── types.ts                 # TypeScript types
│   └── index.ts                 # Main router
├── test/
│   └── routes/
│       └── v1/
│           └── bus-timing.spec.ts # Tests
└── wrangler.toml                # Cloudflare config
```

## CORS

The API supports Cross-Origin Resource Sharing (CORS) and allows requests from any origin (`Access-Control-Allow-Origin: *`).

## Error Handling

The API includes comprehensive error handling for:
- Missing or invalid parameters
- Database connection issues
- Invalid bus stop codes
- No data scenarios

## Technologies
- Cloudflare Workers
- TypeScript
- Supabase
- Vitest for testing
