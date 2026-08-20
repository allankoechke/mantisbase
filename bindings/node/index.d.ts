export interface AppConfig {
  port?: number;
  host?: string;
  dataDir?: string;
  dbType?: string;
  dbUrl?: string;
  /** JWT signing key. Exported as the `MB_JWT_SECRET` environment variable. */
  secretKey?: string;
  publicDir?: string;
  scriptsDir?: string;
  migrationsDir?: string;
  poolSize?: number;
  dev?: boolean;
  skipAdminSetup?: boolean;
}

export type RouteHandler = (req: MantisRequest, res: MantisResponse) => void | Promise<void>;

export class MantisRequest {
  pathParam(name: string): string;
  queryParam(name: string): string;
  header(name: string): string;
  json(): unknown;
  body(): string;
  readonly method: string;
  readonly path: string;
  readonly remoteAddr: string;
}

export class MantisResponse {
  json(statusCode: number, data: unknown): void;
  html(statusCode: number, body: string): void;
  text(statusCode: number, body: string): void;
  send(statusCode: number, body?: string, contentType?: string): void;
  redirect(url: string, status?: number): void;
  setHeader(name: string, value: string): void;
}

export class Router {
  get(path: string, handler: RouteHandler): void;
  post(path: string, handler: RouteHandler): void;
  patch(path: string, handler: RouteHandler): void;
  delete(path: string, handler: RouteHandler): void;
}

export class Database {
  query(sql: string, ...params: Record<string, string | number | boolean | null>[]): Promise<Record<string, unknown>[]>;
  readonly connected: boolean;
}

export class App {
  constructor(opts?: AppConfig);
  start(): void;
  stop(): void;
  readonly router: Router;
  readonly db: Database;
}
