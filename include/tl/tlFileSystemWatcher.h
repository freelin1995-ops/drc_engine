
/*

  KLayout Layout Viewer
  Copyright (C) 2006-2026 Matthias Koefferlein

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef HDR_tlFileSystemWatcher
#define HDR_tlFileSystemWatcher

#if defined(HAVE_QT)

#include "tlEvents.h"

#include <QObject>
#include <QDateTime>

#include <map>
#include <string>
#include <set>

class QTimer;

namespace tl
{

class TL_PUBLIC FileSystemWatcher
  : public QObject
{
Q_OBJECT

public:
  FileSystemWatcher (QObject *parent = 0);
  static void global_enable (bool en);
  void enable (bool en);
  void set_batch_size (size_t n);
  size_t batch_size () const { return m_batch_size; }
  void clear ();
  void add_file (const std::string &path);
  void remove_file (const std::string &path);
  tl::event<const std::string &> file_changed;
  tl::event<const std::string &> file_removed;

signals:
  void fileRemoved (const QString &path);
  void fileChanged (const QString &path);

private slots:
  void timeout ();

private:
  struct FileEntry {
    FileEntry () : refcount (0), size (0) { }
    FileEntry (int r, size_t s, const QDateTime &t) : refcount (r), size (s), time (t) { }
    int refcount;
    size_t size;
    QDateTime time;
  };

  QTimer *m_timer;
  size_t m_batch_size;
  std::map<std::string, FileEntry> m_files;
  std::set<std::string> m_files_removed;
  size_t m_index;
  std::map<std::string, FileEntry>::iterator m_iter;
};

class TL_PUBLIC FileSystemWatcherDisabled
{
public:
  FileSystemWatcherDisabled () { tl::FileSystemWatcher::global_enable (false); }
  ~FileSystemWatcherDisabled () { tl::FileSystemWatcher::global_enable (true); }
};

}

#else

#include "tlEvents.h"
#include <string>

namespace tl
{

class TL_PUBLIC FileSystemWatcher
{
public:
  FileSystemWatcher () {}
  static void global_enable (bool) {}
  void enable (bool) {}
  void set_batch_size (size_t) {}
  size_t batch_size () const { return 0; }
  void clear () {}
  void add_file (const std::string &) {}
  void remove_file (const std::string &) {}
  tl::event<const std::string &> file_changed;
  tl::event<const std::string &> file_removed;
};

class TL_PUBLIC FileSystemWatcherDisabled
{
public:
  FileSystemWatcherDisabled () {}
  ~FileSystemWatcherDisabled () {}
};

}

#endif

#endif
