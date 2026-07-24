import subprocess
import logging

logger = logging.getLogger("PCInterface")

class PCInterface:
    # Path to the compiled C++ service executable
    # Assumes it is installed or located at this path
    BIN_PATH = "/opt/parent-control/service/pc_service"

    @classmethod
    def _run_with_output(cls, args):
        """Helper to run the binary with arguments, returns (success, output)"""
        cmd = [cls.BIN_PATH] + args
        try:
            # check=True will raise CalledProcessError on non-zero exit status
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            return True, result.stdout
        except subprocess.CalledProcessError as e:
            err_msg = e.stderr.strip() or e.stdout.strip()
            logger.error(f"PCService failed: {' '.join(cmd)} | Error: {err_msg}")
            return False, err_msg
        except Exception as e:
            logger.error(f"PCService execution error: {str(e)}")
            return False, str(e)

    @classmethod
    def _run(cls, args):
        """Helper to run the binary, returns success boolean (backward compatibility)"""
        success, _ = cls._run_with_output(args)
        return success

    @classmethod
    def signal_pc_service(cls):
        """Send SIGUSR1 to the pc_service daemon to trigger immediate NFT sync"""
        import os
        import signal
        try:
            # pidof returns a space-separated string of PIDs
            res = subprocess.run(["pidof", "pc_service"], capture_output=True, text=True)
            pids = res.stdout.strip().split()
            if pids:
                for pid in pids:
                    os.kill(int(pid), signal.SIGUSR1)
                logger.info(f"Sent SIGUSR1 to pc_service (PIDs: {', '.join(pids)}) for NFT sync.")
                return True
            else:
                logger.warning("pc_service not running, cannot send SIGUSR1")
                return False
        except Exception as e:
            logger.error(f"Failed to signal pc_service: {e}")
            return False

    class NFT:
        @staticmethod
        def add_to_group(group_name, mac):
            """Trigger full sync instead of direct add to avoid concurrent nft lock issues"""
            return PCInterface.signal_pc_service()

        @staticmethod
        def init():
            """Initialize firewall rules"""
            return PCInterface._run(["--init-nft"])

        @staticmethod
        def sync():
            """Send signal to pc_service to trigger firewall state sync safely"""
            return PCInterface.signal_pc_service()

        @staticmethod
        def get_status():
            """Get status of all groups (returns JSON list)"""
            import json
            try:
                cmd = [PCInterface.BIN_PATH, "--status"]
                res = subprocess.run(cmd, check=True, capture_output=True, text=True)
                return json.loads(res.stdout)
            except Exception as e:
                logger.error(f"Failed to get status: {e}")
                return []

    class AGH:
        @staticmethod
        def get_status():
            """Get AGH system and protection status"""
            import json
            try:
                cmd = [PCInterface.BIN_PATH, "--agh-status"]
                res = subprocess.run(cmd, check=True, capture_output=True, text=True)
                return json.loads(res.stdout)
            except Exception as e:
                logger.error(f"Failed to get AGH status: {e}")
                return {"running": False, "protection_enabled": False, "protection_disabled_duration": 0}

        @staticmethod
        def start():
            """Manually start AdGuard Home service"""
            # Try the native pc_service command first
            res = PCInterface._run(["--agh-start"])
            if not res:
                # Fallback to standard service management if pc_service doesn't handle it
                import subprocess
                try:
                    subprocess.run(["systemctl", "start", "adguardhome"], check=True)
                    return True
                except:
                    try:
                        subprocess.run(["/etc/init.d/adguardhome", "start"], check=True)
                        return True
                    except:
                        pass
            return res

        @staticmethod
        def pause(duration=""):
            """Pause protection (duration: 10m, 1h, tomorrow, or empty for forever)"""
            args = ["--agh-pause"]
            if duration:
                args.append(str(duration))
            return PCInterface._run(args)

        @staticmethod
        def resume():
            """Resume protection"""
            return PCInterface._run(["--agh-resume"])

        @staticmethod
        def sync():
            """Force sync of AdGuard Home group config"""
            return PCInterface._run_with_output(["--sync-agh"])

        @staticmethod
        def analyze_logs():
            """Run AdGuard daily log analysis"""
            return PCInterface._run(["--analysis-agh"])

        @staticmethod
        def sync_logs(date_str=None):
            """Sync AdGuard logs for a specific date (YYYY-MM-DD), default to today"""
            args = ["--sync-agh-logs"]
            if date_str:
                args.append(str(date_str))
            return PCInterface._run(args)

        @staticmethod
        def list_rules():
            """Get all custom filtering rules"""
            import json
            try:
                # Use subprocess run directly to get output
                cmd = [PCInterface.BIN_PATH, "--agh-list-rules"]
                res = subprocess.run(cmd, check=True, capture_output=True, text=True)
                return json.loads(res.stdout)
            except Exception as e:
                logger.error(f"Failed to list rules: {e}")
                return []

        @staticmethod
        def block_domain(domain):
            """Block a domain via custom filtering rule"""
            return PCInterface._run(["--agh-block", str(domain)])

        @staticmethod
        def unblock_domain(domain):
            """Unblock a domain"""
            return PCInterface._run(["--agh-unblock", str(domain)])

    class Entertainment:
        @staticmethod
        def analyze(date=None):
            """Run entertainment detection logic"""
            args = ["--analysis-entertainment"]
            if date:
                args.append(str(date))
            return PCInterface._run(args)
            
    class License:
        @staticmethod
        def checkin():
            """Force license checkin"""
            return PCInterface._run(["--checkin"])

    class System:
        @staticmethod
        def upgrade():
            """Trigger system upgrade script asynchronously"""
            import os
            import subprocess
            script_path = "/opt/parent-control/service/upgrade.sh"
            if os.path.exists(script_path):
                # Using start_new_session=True to detach the process
                subprocess.Popen([script_path], 
                               stdout=subprocess.DEVNULL, 
                               stderr=subprocess.DEVNULL,
                               start_new_session=True)
                return True
            return False
