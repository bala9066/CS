// Shared types for the VDD Audit backend. Mirrors VddRecord from src/mainwindow.h.

export interface ConfigItem {
  fileName: string;      // CI reference / file name from config
  version: string;
  expectedMd5: string;
  documentLink: string;
  component: string;
}

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

  localStatus: string;   // PENDING | MATCH | MISMATCH | MISSING | CONFIG_ONLY | ERROR
  localStatusReason: string;

  source: 'VDD' | 'CONFIG_ONLY';
}

export interface Settings {
  gatewayUrl: string;
  accessToken: string;
  temperature: number;
  maxTokens: number;
  targetModel: string;
}

/** Settings with the token redacted, safe to send to the browser. */
export interface PublicSettings extends Omit<Settings, 'accessToken'> {
  hasAccessToken: boolean;
}

export function makeRecord(partial: Partial<VddRecord> & { id: number }): VddRecord {
  return {
    fileName: '',
    ciReference: '',
    version: '',
    expectedMd5: '',
    expectedCrc32: '',
    calculatedMd5: '',
    calculatedSha1: '',
    calculatedCrc32: '',
    localFileName: '',
    localCiRef: '',
    localVersion: '',
    localFullPath: '',
    configFileName: '',
    configVersion: '',
    configPath: '',
    configComponent: '',
    configPathExists: false,
    configFileFoundAtPath: false,
    localStatus: 'PENDING',
    localStatusReason: '',
    source: 'VDD',
    ...partial,
  };
}
