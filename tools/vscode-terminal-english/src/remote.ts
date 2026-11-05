import { execFile } from 'node:child_process';
import { promisify } from 'node:util';
import type { InputState } from './switcher';

const execute = promisify(execFile);
export type Runner = (args: readonly string[], signal: AbortSignal) => Promise<string>;
const defaultRunner: Runner = async (args, signal) => {
  const { stdout } = await execute('/usr/bin/fcitx5-remote', [...args], {
    encoding: 'utf8', timeout: 1200, maxBuffer: 1024, signal, windowsHide: true,
  });
  return stdout;
};

export class RemoteClient {
  private controller = new AbortController();
  private allowed: () => boolean;
  private runner: Runner;
  constructor(allowed: () => boolean, runner: Runner = defaultRunner) {
    this.allowed = allowed;
    this.runner = runner;
  }
  signal(): AbortSignal { return this.controller.signal; }
  cancel(): void { this.controller.abort(); this.controller = new AbortController(); }
  check(signal: AbortSignal): void {
    if (signal.aborted || !this.allowed()) throw new Error('Transition cancelled');
  }
  private async run(args: string[], signal: AbortSignal): Promise<string> {
    this.check(signal);
    const output = await this.runner(['--check', ...args], signal);
    this.check(signal);
    return output.trim();
  }
  async read(): Promise<InputState> {
    const signal = this.signal();
    const state = await this.run([], signal);
    if (state !== '1' && state !== '2') throw new Error('No usable Fcitx5 state');
    const method = await this.run(['-n'], signal);
    if (!/^[a-zA-Z0-9_.+-]{1,128}$/.test(method)) throw new Error('Invalid method name');
    if (await this.run([], signal) !== state) throw new Error('Fcitx5 state changed during query');
    return { active: state === '2', method };
  }
  async deactivate(): Promise<void> { await this.run(['-c'], this.signal()); }
  async activate(method: string): Promise<void> {
    if (method !== 'openkey-telex' && method !== 'openkey-vni') throw new Error('Unsupported input method');
    const signal = this.signal();
    await this.run(['-s', method], signal);
    await this.run(['-o'], signal);
  }
}
