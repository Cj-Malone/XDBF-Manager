#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <time.h>
#include "titleentrydialog.h"
#include "avatarawarddialog.h"
#include "achievementinjectordialog.h"

Q_DECLARE_METATYPE(Entry*)

//From:
//http://stackoverflow.com/questions/1070497/c-convert-hex-string-to-signed-integer
template<typename T2, typename T1> inline T2 MainWindow::parse_decimal(const T1 &in)
{
    T2 out;
    std::stringstream ss;
    ss << in;
    ss >> out;
    return out;
}
template<typename T2, typename T1> inline T2 MainWindow::parse_hex(const T1 &in)
{
    T2 out;
    std::stringstream ss;
    ss << std::hex << in;
    ss >> out;
    return out;
}
//Ended.

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    desktop_location = QDesktopServices::storageLocation(QDesktopServices::DesktopLocation);
    ObjectRole = Qt::UserRole + 1;

    ui->setupUi(this);

    friendlyNames[0] = "Achievement";
    friendlyNames[1] = "Image";
    friendlyNames[2] = "Setting";
    friendlyNames[3] = "Title";
    friendlyNames[4] = "String";
    friendlyNames[5] = "Avatar Award";

    QStringList columnHeaders;
    columnHeaders.push_back("ID");
    columnHeaders.push_back("Size");
    columnHeaders.push_back("Offset");
    columnHeaders.push_back("Type");

    ui->tableWidget->setColumnCount(columnHeaders.length());
    ui->tableWidget->setHorizontalHeaderLabels(columnHeaders);

    xdbf = NULL;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_triggered()
{
    QString file_name = QFileDialog::getOpenFileName(this, tr("Open File"), desktop_location, "*.gpd");

    if(file_name == NULL)
        return;

    if(xdbf != NULL)
    {
        xdbf->close();
        xdbf = NULL;
    }
    clear_items();

    QByteArray ba = file_name.toAscii();
    const char* name = ba.data();

    try
    {
        xdbf = new XDBF(name);
        Entry *entries = xdbf->get_entries();
        Header *header = xdbf->get_header();

        clear_items();

        for(unsigned int i = 0; i < header->entry_count; i++)
        {
            QString name = QString::fromStdString(Entry_ID_to_string(entries[i].identifier));
            QTableWidgetItem *item = new QTableWidgetItem((name == "") ? "0x" + QString::number(entries[i].identifier, 16).toUpper() : name);
            item->setData(ObjectRole, QVariant::fromValue(&entries[i]));

            ui->tableWidget->insertRow(i);
            ui->tableWidget->setItem(i, 0, item);
            ui->tableWidget->setItem(i, 1, new QTableWidgetItem("0x" + QString::number(entries[i].length, 16).toUpper()));
            ui->tableWidget->setItem(i, 2, new QTableWidgetItem("0x" + QString::number(entries[i].address, 16).toUpper()));

            QString setting_entry_name = "";
            if(entries[i].type == ET_SETTING)
            {
                Setting_Entry *entry = xdbf->get_setting_entry(&entries[i]);
                setting_entry_name = " - " + QString::fromStdString(xdbf->get_setting_entry_name(entry));
            }
            else if (entries[i].identifier == 0x8000 && entries[i].type == ET_STRING)
            {
                wchar_t *titleName = (wchar_t*)xdbf->extract_entry(&entries[i]);
                SwapEndianUnicode(titleName, entries[i].length);
                setWindowTitle("XDBF Manager - " + QString::fromWCharArray(titleName));
            }
            ui->tableWidget->setItem(i, 3, new QTableWidgetItem(friendlyNames[entries[i].type - 1] + setting_entry_name));
        }
    }
    catch(char *exception)
    {
        QMessageBox::warning(this, "Error", exception, QMessageBox::Ok);
    }
}

void MainWindow::clear_items()
{
    for (int i = ui->tableWidget->rowCount() - 1; i >= 0; --i)
        ui->tableWidget->removeRow(i);
}

void MainWindow::on_pushButton_clicked()
{
    if(ui->tableWidget->selectedItems().count() < 1)
        return;

    QList<QTableWidgetItem*> list = ui->tableWidget->selectedItems();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select a Directory"), desktop_location) + "\\";

    if(dir == "\\")
        return;

    for(int i = 0; i < list.size() / ui->tableWidget->columnCount(); i++)
    {
        FILE *f;

        QString path = dir + ui->tableWidget->item(list[i]->row(), 0)->text().remove("0x");
        QByteArray ba = path.toAscii();
        char *path_c = ba.data();
        fopen_s(&f, path_c, "wb");

        try
        {
            Entry *entry = list[i]->data(ObjectRole).value<Entry*>();
            char *data = xdbf->extract_entry(entry);
            fwrite(data, entry->length, sizeof(char), f);
            fclose(f);
        }
        catch(char *exce)
        {
            QMessageBox::information(this, "Error Occurred", exce, QMessageBox::Ok);
        }
    }

    QMessageBox::information(this, "Extraction Successful", "All selected files have been extracted successfully!", QMessageBox::Ok);
}

void MainWindow::on_actionClose_triggered()
{
    if(xdbf == NULL)
        return;

    xdbf->close();
    xdbf = NULL;
    clear_items();
}

void MainWindow::on_tableWidget_doubleClicked(const QModelIndex &index)
{
    QTableWidgetItem* item = ui->tableWidget->selectedItems()[0];
    Entry *e = item->data(ObjectRole).value<Entry*>();

    if(e->identifier == SYNC_LIST || e->identifier == SYNC_DATA || ((e->identifier == 1 || e->identifier == 2) && e->type == ET_AVATAR_AWARD))
    {
        if(e->identifier == SYNC_LIST || e->identifier == 1)
        {
            Sync_List sl = xdbf->get_sync_list(e->type, e->identifier);
            SyncListDialog dialog(this, &sl, xdbf);
            dialog.exec();
        }
        else
        {

        }
        return;
    }
    else if (e->type == ET_SETTING)
    {
        Setting_Entry *se = xdbf->get_setting_entry(e);

        QString title = QString::fromStdString(XDBF::get_setting_entry_name(se)) + " Data";
        const QString message_header = "<html><center><h3>" + title + " (Setting Entry)</h3></center><hr /><br />";
        switch (se->type)
        {
            case SET_INT32:
                showIntMessageBox(se->i32_data, message_header, title);
                break;
            case SET_INT64:
                showIntMessageBox(se->i64_data, message_header, title);
                break;
            case SET_FLOAT:
                showFloatMessageBox(se->float_data, message_header, title);
                break;
            case SET_DOUBLE:
                showFloatMessageBox(se->double_data, message_header, title);
                break;
            case SET_DATETIME:
                showDatetimeMessageBox(se->time_stamp, message_header, title);
                break;
            case SET_UNICODE:
                showStringMessageBox(se->unicode_string.str, message_header, title);
                break;
            case SET_BINARY:
                BinaryDialog bd(this, se);
                bd.exec();
                break;
        }
    }
    else if (e->type == ET_STRING)
    {
        wchar_t *str = (wchar_t*)xdbf->extract_entry(e);
        SwapEndianUnicode(str, e->length);
        showStringMessageBox(str, "<html><center><h3>String Entry</h3></center><hr /><br />", "String Entry");
    }
    else if (e->type == ET_IMAGE)
    {
        unsigned char *img_data = (unsigned char*)xdbf->extract_entry(e);
        QImage img = QImage::fromData(img_data, e->length);
        ImageDialog im(this, &img);
        im.exec();
    }
    else if (e->type == ET_TITLE)
    {
        Title_Entry *tent = xdbf->get_title_entry(e);
        if (tent == NULL)
            return;

        TitleEntryDialog dialog(this, tent);
        dialog.exec();
    }
    else if (e->type == ET_ACHIEVEMENT)
    {
        Achievement_Entry *chiev = xdbf->get_achievement_entry(e);
        Entry *entries = xdbf->get_entries();
        Entry *imageEntry = NULL;

        for (int i = 0; i < xdbf->get_header()->entry_count; i++)
            if(entries[i].type == ET_IMAGE && entries[i].identifier == chiev->imageID )
                imageEntry = &entries[i];

        AchievementViewer *dialog;
        if (imageEntry != NULL)
            dialog = new AchievementViewer(this, chiev, xdbf->get_file(), QImage::fromData((unsigned char*)xdbf->extract_entry(imageEntry), imageEntry->length), e->address);
        else
            dialog = new AchievementViewer(this, chiev, xdbf->get_file(), QImage(":/images/HiddenAchievement.png"), e->address);

        dialog->exec();
    }
    else if (e->type == ET_AVATAR_AWARD)
    {
        Avatar_Award_Entry *award = xdbf->get_avatar_award_entry(e);
        AvatarAwardDialog dialog(this, award, xdbf);
        dialog.exec();
    }
}

void MainWindow::showIntMessageBox(unsigned long long num, QString message_header, QString title)
{
    QString decimal = "<b>Decimal:</b> " + QString::number(num);
    QString hex = "<b>Hexadecimal:</b> 0x" + QString::number(num, 16);
    QString octal = "<b>Octal:</b> 0" + QString::number(num, 8) + "</html>";

    QMessageBox::about(this, title, message_header + decimal + "<br />" + hex + "<br />" + octal);
}

void MainWindow::showFloatMessageBox(double num, QString message_header, QString title)
{
    QString standard = "<b>Standard:</b> " + QString::number(num);
    char nipples[25];
    sprintf(nipples, "%E\0", num);
    QString scientificNotation = "<b>Scientific Notation: </b>" + QString::fromAscii(nipples);
    QMessageBox::about(this, title, message_header + standard + "<br />" + scientificNotation + "</html>");
}

void MainWindow::showStringMessageBox(wchar_t *wStr, QString message_header, QString title)
{
    int strLen = QString::fromWCharArray(wStr).length();
    QString uni_str = "<b>Unicode String:</b> " + ((strLen == 0) ? "<i>Null</i>" : QString::fromWCharArray(wStr));
    QString uni_len = "<b>String Length:</b> " + QString::number(strLen) + "</html>";

    QMessageBox::about(this, title, message_header + uni_str + "<br />" + uni_len);
}

void MainWindow::showDatetimeMessageBox(FILETIME time, QString message_header, QString title)
{
    QString time_str = "<b>Datetime:</b> " + QString::fromStdString(XDBF::FILETIME_to_string(&time)) + "</html>";
    QMessageBox::about(this, title, message_header + time_str);
}

void MainWindow::on_pushButton_2_clicked()
{
    AchievementInjectorDialog dialog(this, xdbf);
    dialog.exec();

   /* Achievement_Entry entry = {0};
    entry.size = 0x1C;
    entry.id = 1337;
    entry.imageID = 1337;
    entry.gamerscore = 1337;
    entry.name = L"Fleck's Achievement";
    entry.lockedDescription = L"What do you think?";
    entry.unlockedDescription = L"Hm! Looks like a good effort, but I think you could improve.";

    xdbf->injectAchievementEntry(&entry); */
}
