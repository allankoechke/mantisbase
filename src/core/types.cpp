#include "../../include/mantisbase/core/types.h"
#include "../../include/mantisbase/mantisbase.h"

namespace mb {
    IMantisBase::IMantisBase(const MantisBase& app)
    : m_app(app) {}

    const MantisBase& IMantisBase::mbApp() const {
        return m_app;
    }

    const Logger& IMantisBase::logger() const {
        return m_app.logger();
    }
}