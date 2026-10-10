#pragma once

// UI-thread-only reentrancy gate. Not an interprocess lock or a transaction.
namespace OpenKeyUI
{
    class OperationGate
    {
    public:
        class Lease
        {
        public:
            Lease(Lease const&) = delete;
            Lease& operator=(Lease const&) = delete;
            ~Lease() { Release(); }
            explicit operator bool() const noexcept { return m_owner != nullptr; }
            void Release() noexcept
            {
                if (m_owner) { m_owner->m_busy = false; m_owner = nullptr; }
            }
        private:
            friend class OperationGate;
            explicit Lease(OperationGate* owner) noexcept : m_owner(owner)
            {
                if (m_owner) m_owner->m_busy = true;
            }
            OperationGate* m_owner;
        };
        OperationGate() = default;
        OperationGate(OperationGate const&) = delete;
        OperationGate& operator=(OperationGate const&) = delete;
        bool Busy() const noexcept { return m_busy; }
        Lease TryBegin() noexcept { return Lease{m_busy ? nullptr : this}; }
    private:
        bool m_busy = false;
    };
}
