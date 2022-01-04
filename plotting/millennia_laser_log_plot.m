log_filename = 'log_2675.txt';

if ~exist(log_filename,"file")
    error("The file '%s' could not be found. Check that the file is in the current directory or in the MATLAB path.",log_filename);
end

log_file_id = fopen(log_filename);

log_file_data_txt = fread(log_file_id,'*char').';

comment_idxs = find(log_file_data_txt == '#');

mask = true(size(log_file_data_txt));

lasername = '';

% Parse log file and removes comments
for comment_idx = comment_idxs
    for idx = (comment_idx+1):length(log_file_data_txt)
        
        if log_file_data_txt(idx) == newline || log_file_data_txt(idx) == char(13)
            if log_file_data_txt(idx+1) == newline || log_file_data_txt(idx+1) == char(13)
                idx1 = idx+1;
            else
                idx1 = idx;
            end
            
            % Extract laser name from log file
            if isempty(lasername)
                started_logging_txt = log_file_data_txt(comment_idx+(24:38));
                if ~isempty(started_logging_txt) && strcmp(started_logging_txt,'Started logging')
                    for idx2 = idx1:-1:comment_idx+40
                        if log_file_data_txt(idx2) ~= newline && log_file_data_txt(idx2) ~= char(13)
                            lasername = log_file_data_txt(comment_idx+40:idx2);
                            break;
                        end
                    end
                end
                
            end
            mask(comment_idx:idx1) = false;
            break
        end
    end
end

log_file_data_txt = log_file_data_txt(mask);

fclose(log_file_id);

% Timestamp; Power; RMS	Avg.; RMS; T-dio.; T.cry.; Dio.c.; Dio.h.; Head-h.; Las.-h.; Error
log_file_data = textscan(log_file_data_txt,'%{yyyy-MM-dd HH:mm:ss}D %f %f %f %f %f %f %f %f %f %f','Delimiter','\t');

% Plot log data
figure; clf;
sgtitle(['Log data of ',lasername]);

subplot(3,2,1);
plot(log_file_data{1},log_file_data{2},'MarkerEdgeColor','none','MarkerFaceColor',[0,114,189]/255,'MarkerSize',2,'LineStyle','none','Marker','o')
ylabel('Power (W)');

subplot(3,2,2);
plot(log_file_data{1},log_file_data{4},'MarkerEdgeColor','none','MarkerFaceColor',[0,114,189]/255,'MarkerSize',4,'LineStyle','none','Marker','o')
ylabel('RIN (%)');
ylim([0,0.2]);

subplot(3,2,3);
plot(log_file_data{1},log_file_data{5},'MarkerEdgeColor','none','MarkerFaceColor',[0,114,189]/255,'MarkerSize',4,'LineStyle','none','Marker','o')
ylabel('Temp. diodes');

subplot(3,2,4);
plot(log_file_data{1},log_file_data{6},'MarkerEdgeColor','none','MarkerFaceColor',[0,114,189]/255,'MarkerSize',4,'LineStyle','none','Marker','o')
ylabel('Temp. crystal');

subplot(3,2,5);
plot(log_file_data{1},log_file_data{7},'MarkerEdgeColor','none','MarkerFaceColor',[0,114,189]/255,'MarkerSize',4,'LineStyle','none','Marker','o')
ylabel('Diode current');

subplot(3,2,6);
plot(log_file_data{1},log_file_data{8},'MarkerEdgeColor','none','MarkerFaceColor',[0,114,189]/255,'MarkerSize',4,'LineStyle','none','Marker','o')
ylabel('Diode hours');



