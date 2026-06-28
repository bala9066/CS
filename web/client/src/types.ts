// Client-side mirror of the backend VddRecord and settings types.

export interface VddRecord {
  id: number;
  fileName: string;
  ciReference: string;
  version: string;
  expectedMd5: string;
  expectedCrc32: string;
  calculatedMd5: string;
  calculatedSha1: string;
  calculatedCrc32: string;
  localFileName: string;
  localCiRef: string;
  localVersion: string;
  localFullPath: string;
  configFileName: string;
  configVersion: string;
  configPath: string;
  configComponent: string;
  configPathExists: boolean;
  configFileFoundAtPath: boolean;
  localStatus: string;
  localStatusReason: string;
  source: 'VDD' | 'CONFIG_ONLY';
}

export interface PublicSettings {
  gatewayUrl: string;
  temperature: number;
  maxTokens: number;
  targetModel: string;
  hasAccessToken: boolean;
}

export interface ChecksumResult {
  path: string;
  success: boolean;
  md5: string;
  sha1: string;
  crc32: string;
  error?: string;
  sizeBytes?: number;
}

export type LogType = 'info' | 'success' | 'warning' | 'error';
export interface LogEntry {
  time: string;
  type: LogType;
  msg: string;
}
