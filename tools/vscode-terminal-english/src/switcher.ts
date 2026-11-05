export interface InputState {
  active: boolean;
  method: string;
}

export interface SwitchPort {
  isFocused(): boolean;
  focus(target: 'terminal' | 'editor'): Promise<void>;
  read(): Promise<InputState>;
  deactivate(): Promise<void>;
  activate(method: string): Promise<void>;
}

const cancelled = Symbol('cancelled');

function isOpenKey(method: string): boolean {
  return method === 'openkey-telex' || method === 'openkey-vni';
}

export class TerminalSwitcher {
  private readonly port: SwitchPort;
  private snapshot: InputState | undefined;
  private epoch = 0;
  private pending = false;
  private disposed = false;

  constructor(port: SwitchPort) {
    this.port = port;
  }

  enter(): Promise<string> {
    return this.run(async (epoch) => {
      if (this.snapshot !== undefined) return 'already-terminal';

      const original = await this.operation(epoch, () => this.port.read());
      // Copy the state: the adapter must not be able to mutate our snapshot.
      const snapshot = { active: original.active, method: original.method };
      if (snapshot.active && !isOpenKey(snapshot.method)) return 'unsupported';

      await this.operation(epoch, () => this.port.focus('terminal'));
      const terminal = await this.operation(epoch, () => this.port.read());
      if (terminal.active) {
        await this.operation(epoch, () => this.port.deactivate());
      }
      const verified = await this.operation(epoch, () => this.port.read());
      if (verified.active) throw new Error('Terminal input is still active');
      this.snapshot = snapshot;
      return 'entered';
    });
  }

  leave(): Promise<string> {
    return this.run(async (epoch) => {
      const snapshot = this.snapshot;
      this.snapshot = undefined;
      if (snapshot === undefined) {
        await this.operation(epoch, () => this.port.focus('editor'));
        return 'no-snapshot';
      }

      const terminal = await this.operation(epoch, () => this.port.read());
      if (terminal.active) {
        await this.operation(epoch, () => this.port.focus('editor'));
        return 'changed';
      }

      await this.operation(epoch, () => this.port.focus('editor'));
      const editor = await this.operation(epoch, () => this.port.read());
      if (!snapshot.active) return 'unchanged';
      if (editor.active) {
        return editor.method === snapshot.method ? 'unchanged' : 'changed';
      }

      await this.operation(epoch, () => this.port.activate(snapshot.method));
      const verified = await this.operation(epoch, () => this.port.read());
      if (!verified.active || verified.method !== snapshot.method) {
        throw new Error('Editor input restoration was not confirmed');
      }
      return 'restored';
    });
  }

  invalidate(): void {
    this.epoch += 1;
    this.snapshot = undefined;
  }

  dispose(): void {
    this.disposed = true;
    this.invalidate();
  }

  private guard(epoch: number): void {
    if (this.disposed || epoch !== this.epoch) throw cancelled;
    if (!this.port.isFocused()) {
      this.invalidate();
      throw cancelled;
    }
  }

  private async operation<T>(epoch: number, action: () => Promise<T>): Promise<T> {
    this.guard(epoch);
    const result = await action();
    this.guard(epoch);
    return result;
  }

  private async run(action: (epoch: number) => Promise<string>): Promise<string> {
    if (this.pending) return 'busy';
    this.pending = true;
    try {
      const epoch = this.epoch;
      this.guard(epoch);
      const status = await action(epoch);
      this.guard(epoch);
      return status;
    } catch (error) {
      this.snapshot = undefined;
      return error === cancelled ? 'cancelled' : 'error';
    } finally {
      this.pending = false;
    }
  }
}
