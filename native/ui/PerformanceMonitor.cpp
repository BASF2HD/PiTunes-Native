#include "PerformanceMonitor.h"
#include "coverflow/CoverFlowWidget.h"
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QSaveFile>
#include <QSysInfo>
#include <array>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace PiTunes {
namespace {
using Counter = qint64;
constexpr int maxSamples = 900;

QByteArray read(const QString &path, int limit = 32768) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(limit) : QByteArray();
}

QByteArray oneLine(const QString &path) {
    auto bytes = read(path, 256);
    if (const int nul = bytes.indexOf('\0'); nul >= 0) bytes.truncate(nul);
    return bytes.simplified();
}

QString number(double value, int precision = 1) {
    return QString::number(value, 'f', precision);
}

QString formatMetric(double value, const QString &unit, int precision = 1) {
    return value < 0. ? QStringLiteral("n/a") : number(value, precision) + unit;
}

struct Cpu { Counter total = 0, busy = 0; };
struct Disk { Counter read = 0, written = 0; };
struct Process {
    int pid = 0;
    double cpu = -1.;
    Counter rssKiB = -1;
    Counter ticks = 0;
    Counter start = 0;
};
struct Sample {
    QVector<double> cpu;
    std::array<Process, 4> processes;
    Counter ram = 0, available = 0, swap = 0, swapFree = 0;
    Counter zramRaw = -1, zramMemory = -1, swapIn = -1, swapOut = -1;
    double temp = -273.15, frequency = -1., reads = -1., writes = -1., uptime = -1.;
    double totalCpu() const { return cpu.isEmpty() ? -1. : cpu.first(); }
    Counter used() const { return qMax<Counter>(0, ram - available); }
};
}

struct PerformanceMonitor::Sampler {
    QHash<QByteArray, Cpu> previousCpu;
    QHash<QByteArray, Disk> previousDisks;
    std::array<Process, 4> processes;
    QString thermalPath;
    QString model, system;
    Counter pageKiB = 0, swapIn = -1, swapOut = -1;
    int discoveryCountdown = 0;
    bool memoryAvailable = false;

    Sampler() {
#ifdef Q_OS_LINUX
        pageKiB = sysconf(_SC_PAGESIZE) / 1024;
        processes[0].pid = QCoreApplication::applicationPid();
        auto info = oneLine("/sys/class/dmi/id/product_name");
        if (info.isEmpty()) info = oneLine("/proc/device-tree/model");
        model = QString::fromUtf8(info);
        QDirIterator sensors("/sys/class/thermal", {"thermal_zone*"}, QDir::Dirs | QDir::NoDotAndDotDot);
        while (sensors.hasNext()) {
            const auto zone = sensors.next();
            auto type = oneLine(zone + "/type").toLower();
            if (type == "x86_pkg_temp" || type == "cpu-thermal" || type == "cpu_thermal") {
                thermalPath = zone + "/temp";
                break;
            }
        }
        if (thermalPath.isEmpty()) {
            QDirIterator hwmon("/sys/class/hwmon", {"hwmon*"}, QDir::Dirs | QDir::NoDotAndDotDot);
            while (hwmon.hasNext()) {
                const auto path = hwmon.next();
                const auto driver = oneLine(path + "/name");
                if (driver == "coretemp" || driver == "k10temp") {
                    thermalPath = path + "/temp1_input";
                    break;
                }
            }
        }
#endif
        system = QSysInfo::prettyProductName() + " (" + QSysInfo::currentCpuArchitecture() + ')';
    }

    Process process(int pid) const {
        Process p;
        p.pid = pid;
        if (pid <= 0) return p;
        const auto stat = read("/proc/" + QString::number(pid) + "/stat", 4096);
        const int end = stat.lastIndexOf(')');
        if (end < 0) return p;
        const auto fields = stat.mid(end + 1).simplified().split(' ');
        if (fields.size() < 22) return p;
        p.ticks = fields[11].toLongLong() + fields[12].toLongLong();
        p.start = fields[19].toLongLong();
        if (pageKiB > 0) p.rssKiB = fields[21].toLongLong() * pageKiB;
        return p;
    }

    void discoverProcesses() {
        QDirIterator pids("/proc", QDir::Dirs | QDir::NoDotAndDotDot);
        while (pids.hasNext()) {
            const auto path = pids.next();
            bool numeric;
            const int pid = pids.fileName().toInt(&numeric);
            if (!numeric || pid <= 0) continue;
            const auto name = oneLine(path + "/comm");
            if (name == "mpd" && !processes[2].pid) processes[2].pid = pid;
            if (name == "Xorg" && !processes[3].pid) processes[3].pid = pid;
            if (name.startsWith("python") && !processes[1].pid) {
                const auto args = read(path + "/cmdline", 2048).split('\0');
                if (args.contains("/opt/pitunes/backend/server.py")) processes[1].pid = pid;
            }
            if (processes[1].pid && processes[2].pid && processes[3].pid) break;
        }
    }

    Sample collect(bool detailed, double seconds) {
        Sample result;
        const auto memory = read("/proc/meminfo", 8192).split('\n');
        memoryAvailable = false;
        for (const auto &line : memory) {
            const auto fields = line.simplified().split(' ');
            if (fields.size() < 2) continue;
            const Counter value = fields[1].toLongLong();
            if (fields[0] == "MemTotal:") result.ram = value;
            if (fields[0] == "MemAvailable:") { result.available = value; memoryAvailable = true; }
            if (fields[0] == "SwapTotal:") result.swap = value;
            if (fields[0] == "SwapFree:") result.swapFree = value;
        }
        Counter cpuTicks = 0;
        QHash<QByteArray, Cpu> cpu;
        for (const auto &line : read("/proc/stat", 65536).split('\n')) {
            if (!line.startsWith("cpu")) break;
            const auto fields = line.simplified().split(' ');
            if (fields.size() < 5) continue;
            Cpu now;
            for (int i = 1; i < qMin(9, fields.size()); ++i) {
                const auto ticks = fields[i].toLongLong();
                now.total += ticks;
                if (i != 4 && i != 5) now.busy += ticks; // Idle and I/O wait.
            }
            const auto &name = fields.first();
            const auto old = previousCpu.value(name);
            const auto delta = now.total - old.total;
            if (name == "cpu" && old.total > 0) cpuTicks = delta;
            result.cpu.append(old.total && delta > 0
                ? qBound(0., 100. * (now.busy - old.busy) / delta, 100.) : -1.);
            cpu.insert(name, now);
            if (!detailed) break;
        }
        previousCpu = std::move(cpu);
        if (!thermalPath.isEmpty()) {
            bool ok;
            const double milliC = oneLine(thermalPath).toDouble(&ok);
            if (ok && milliC > -50000. && milliC < 150000.) result.temp = milliC / 1000.;
        }
        if (!detailed) return result;

        if (discoveryCountdown-- <= 0) {
            discoverProcesses();
            discoveryCountdown = 14;
        }
        for (size_t i = 0; i < processes.size(); ++i) {
            const auto old = processes[i];
            auto p = process(old.pid);
            if (p.start && p.start == old.start && cpuTicks > 0)
                p.cpu = qBound(0., 100. * (p.ticks - old.ticks) / cpuTicks, 100.);
            if (!p.start && i != 0) p.pid = 0;
            processes[i] = result.processes[i] = p;
        }

        QHash<QByteArray, Disk> disks;
        double sectorsRead = 0., sectorsWritten = 0.;
        bool matched = false;
        for (const auto &line : read("/proc/diskstats", 65536).split('\n')) {
            const auto fields = line.simplified().split(' ');
            if (fields.size() < 10) continue;
            const auto &name = fields[2];
            if (name.startsWith("zram")) {
                const auto z = oneLine("/sys/block/" + QString::fromLatin1(name) + "/mm_stat").split(' ');
                if (z.size() >= 3) {
                    if (result.zramRaw < 0) result.zramRaw = result.zramMemory = 0;
                    result.zramRaw += z[0].toLongLong() / 1024;
                    result.zramMemory += z[2].toLongLong() / 1024;
                }
            }
            if (name.startsWith("loop") || name.startsWith("ram") || name.startsWith("zram")
                || name.startsWith("dm-") || name.startsWith("md")
                || !QFile::exists("/sys/block/" + QString::fromLatin1(name))) continue;
            const auto key = fields[0] + ':' + fields[1] + ':' + name;
            const Disk now{fields[5].toLongLong(), fields[9].toLongLong()};
            disks.insert(key, now);
            const auto old = previousDisks.constFind(key);
            if (old == previousDisks.cend() || now.read < old->read || now.written < old->written) continue;
            sectorsRead += now.read - old->read;
            sectorsWritten += now.written - old->written;
            matched = true;
        }
        previousDisks = std::move(disks);
        if (matched && seconds > 0.) {
            result.reads = sectorsRead / 2. / seconds;
            result.writes = sectorsWritten / 2. / seconds;
        }
        for (const auto &line : read("/proc/vmstat", 16384).split('\n')) {
            if (line.startsWith("pswpin ") || line.startsWith("pswpout ")) {
                const bool isRead = line.startsWith("pswpin ");
                auto &previous = isRead ? swapIn : swapOut;
                const auto now = line.mid(line.indexOf(' ') + 1).toLongLong();
                if (previous >= 0 && now >= previous)
                    (isRead ? result.swapIn : result.swapOut) = (now - previous) * pageKiB;
                previous = now;
            }
        }
        bool ok;
        const auto khz = oneLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq").toDouble(&ok);
        if (ok && khz > 0.) result.frequency = khz / 1000.;
        const auto uptime = oneLine("/proc/uptime").split(' ').first().toDouble(&ok);
        if (ok) result.uptime = uptime;
        return result;
    }
};

PerformanceMonitor::PerformanceMonitor(CoverFlowWidget *flow, QWidget *parent)
    : QLabel(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus
                           | Qt::WindowTransparentForInput), m_flow(flow) {
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setTextFormat(Qt::PlainText);
    setMargin(7);
    setStyleSheet("background: #17191f; color: #e5e7ed; font-size: 11px;");
    setAccessibleName("System Performance Monitor");
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &PerformanceMonitor::refresh);
}

PerformanceMonitor::~PerformanceMonitor() {
    if (recording()) saveRecording();
}

void PerformanceMonitor::setMode(MonitorMode mode) {
    if (mode == m_mode) return;
    m_mode = mode;
    if (mode == MonitorMode::Off) {
        m_timer.stop();
        m_flow->setFrameSampling(false);
        stopRecording();
        m_sampler.reset();
        hide();
    } else {
        if (!m_sampler) {
            m_sampler = std::make_unique<Sampler>();
            m_flow->setFrameSampling(true);
            m_interval.start();
            m_timer.start();
        }
        refresh();
        show();
    }
    emit modeChanged(int(mode));
}

void PerformanceMonitor::startRecording(const QString &path) {
    if (path.isEmpty() || recording()) return;
    m_csvPath = path;
    m_rows = 0;
    m_csv = "utc,cpu_pct,ram_used_kib,ram_total_kib,animation_fps,frame_count,active_animation_ms,avg_frame_ms,peak_frame_ms,disk_read_kib_s,disk_write_kib_s,swap_in_kib,swap_out_kib,first_cover_paint_ms\n";
    if (m_mode == MonitorMode::Off) setMode(MonitorMode::Compact);
    emit recordingChanged(true);
}

void PerformanceMonitor::saveRecording() {
    QSaveFile file(m_csvPath);
    const bool saved = file.open(QIODevice::WriteOnly) && file.write(m_csv) == m_csv.size() && file.commit();
    const auto path = m_csvPath;
    m_csvPath.clear();
    m_csv.clear();
    emit recordingChanged(false);
    emit recordingSaved(path, saved);
}

void PerformanceMonitor::stopRecording() {
    if (recording()) saveRecording();
}

void PerformanceMonitor::refresh() {
    const double seconds = qMax(1.e-6, m_interval.nsecsElapsed() / 1.e9);
    m_interval.restart();
    const bool detailed = m_mode == MonitorMode::Detailed;
    const auto sample = m_sampler->collect(detailed || recording(), seconds);
    const auto frame = m_flow->takeFrameSample();
    const double fps = frame.activeNs > 0 ? frame.count * (1.e9 / frame.activeNs) : 0.;
    const double average = frame.count ? frame.totalNs / (frame.count * 1.e6) : 0.;
    const double peak = frame.peakNs / 1.e6;
    const bool ramKnown = sample.ram > 0 && m_sampler->memoryAvailable;
    const auto cpu = formatMetric(sample.totalCpu(), "%");
    const auto memory = ramKnown ? number(sample.used() / 1024., 0) + " / " + number(sample.ram / 1024., 0) + " MiB" : QString("n/a");
    const auto frames = frame.count ? number(fps) + " FPS (animation)" : QString("0 FPS (idle)");
    const auto temp = sample.temp > -273.15 ? number(sample.temp) + " °C" : QString("n/a");
    QString display = "CPU " + cpu + "  RAM " + memory + "  " + frames + "  CPU " + temp;
    if (detailed) {
        display = "CPU " + cpu;
        for (int i = 1; i < sample.cpu.size() && i <= 32; ++i) {
            if ((i - 1) % 4 == 0) display += '\n';
            display += QString("  %1: %2").arg(i - 1).arg(formatMetric(sample.cpu[i], "%", 0));
        }
        display += "\nRAM " + memory;
        if (ramKnown) display += QString(" (%1%, %2 MiB available)").arg(number(100. * sample.used() / sample.ram))
                                                                  .arg(number(sample.available / 1024., 0));
        display += sample.ram > 0 ? QString("\nSwap %1 / %2 MiB").arg(number((sample.swap - sample.swapFree) / 1024.))
                                                                .arg(number(sample.swap / 1024.)) : QString("\nSwap n/a");
        display += sample.zramRaw >= 0 ? QString("  ZRAM %1 MiB data / %2 MiB resident")
            .arg(number(sample.zramRaw / 1024.)).arg(number(sample.zramMemory / 1024.)) : QString("  ZRAM n/a");
        display += "\nCPU " + temp + "  " + formatMetric(sample.frequency, " MHz", 0);
        const std::array<const char *, 4> labels{"PiTunes Native", "Python backend", "MPD", "Xorg"};
        for (size_t i = 0; i < labels.size(); ++i) {
            const auto &p = sample.processes[i];
            display += QString("\n%1: %2 RSS  %3 CPU").arg(labels[i])
                .arg(formatMetric(p.rssKiB < 0 ? -1. : p.rssKiB / 1024., " MiB"), formatMetric(p.cpu, "%"));
        }
        display += QString("\nCoverFlow %1  avg %2 ms  peak %3 ms").arg(frames, number(average, 2), number(peak, 2));
        display += QString("\nDisk %1 read  %2 write").arg(formatMetric(sample.reads, " KiB/s"), formatMetric(sample.writes, " KiB/s"));
        display += "\nFirst cover " + formatMetric(m_flow->firstCoverPaintMs(), " ms", 0);
        display += "\nUptime " + formatMetric(sample.uptime / 60., " min") + "\n" + m_sampler->system
            + "\n" + (m_sampler->model.isEmpty() ? QString("Model unavailable") : m_sampler->model);
    }
    if (recording()) display.prepend(QString("Recording %1 / %2 samples\n").arg(m_rows).arg(maxSamples));
    setText(display);
    const int panelWidth = qMax(1, qMin(detailed ? 398 : 346, parentWidget()->width() - 16));
    const int panelHeight = (display.count('\n') + 1) * fontMetrics().lineSpacing() + 2 * margin() + 2;
    if (size() != QSize(panelWidth, panelHeight)) setFixedSize(panelWidth, panelHeight);
    const auto panelPosition = parentWidget()->mapToGlobal(QPoint(parentWidget()->width() - width() - 8, 38));
    if (pos() != panelPosition) move(panelPosition);

    if (recording()) {
        const auto cell = [](double value) { return value < 0. ? QByteArray() : QByteArray::number(value, 'f', 2); };
        m_csv += QStringList{QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs), QString::fromLatin1(cell(sample.totalCpu())),
            ramKnown ? QString::number(sample.used()) : QString(), ramKnown ? QString::number(sample.ram) : QString(),
            number(fps, 2), QString::number(frame.count), number(frame.activeNs / 1.e6, 2), number(average, 2), number(peak, 2),
            QString::fromLatin1(cell(sample.reads)), QString::fromLatin1(cell(sample.writes)),
            QString::fromLatin1(cell(sample.swapIn)), QString::fromLatin1(cell(sample.swapOut)),
            m_flow->firstCoverPaintMs() < 0 ? QString() : QString::number(m_flow->firstCoverPaintMs())}.join(',').toUtf8() + '\n';
        if (++m_rows >= maxSamples) stopRecording();
    }
}
} // namespace PiTunes
